/*
 * Copyright (c) 2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <map>

#include "dcgm_nvswitch_structs.h"

#include <DcgmCoreProxy.h>
#include <DcgmMutex.h>
#include <DcgmWatchTable.h>
#include <dcgm_nscq.h>
#include <dcgm_structs.h>

namespace DcgmNs
{

using uuid_p       = nscq_uuid_t *;
using link_id_t    = uint8_t;
using lane_vc_id_t = uint8_t;

class DcgmNvSwitchFieldWatch
{
public:
    dcgm_field_entity_group_t entityGroupId;
    unsigned int entityId;
    unsigned short fieldId;
    timelib64_t updateIntervalUsec;
};

class DcgmNvSwitchError
{
public:
    unsigned int error = 0;
    uint64_t timestamp = 0;
    int port           = -1; // -1 indicates not port-specific
};

class DcgmNvSwitchManager
{
public:
    using phys_id_t      = uint32_t;
    using uuid_p         = nscq_uuid_t *;
    using label_t        = nscq_label_t;
    using link_id_t      = uint8_t;
    using lane_vc_id_t   = uint8_t;
    using nvlink_state_t = nscq_nvlink_state_t;

    /*************************************************************************/
    explicit DcgmNvSwitchManager(dcgmCoreCallbacks_t *dcc);

    /*************************************************************************/
    ~DcgmNvSwitchManager();

    /*************************************************************************/
    /*
     * Attach to NSCQ and scan for NvSwitches + link statuses
     *
     * Returns DCGM_ST_OK on success.
     *         DCGM_ST_? on error.
     */
    dcgmReturn_t AttachToNscq();

    /*************************************************************************/
    /*
     * Detach from NSCQ, unloading its driver.
     *
     * Returns DCGM_ST_OK on success.
     *         DCGM_ST_? on error.
     */
    dcgmReturn_t DetachFromNscq();

    /*************************************************************************/
    /**
     * Populate switchIds with the ids of each NvSwitch present on the system.
     *
     * @param count[in/out]    - passed in as the maximum number of switch ids that can be placed in switchIds. Set
     *                           to the number of switch ids placed in switchIds
     * @param switchIds[out]   - populated with the physical ids of the NvSwitches on the host.
     * @param flags            - specify a characteristic to get a subset of switch ids placed in switchIds
     *                           (unused for now)
     *
     * @return DCGM_ST_OK:                all desired switch ids added to buffer
     *         DCGM_ST_INSUFFICIENT_SIZE: the buffer wasn't big enough to add all desired switch ids
     */
    dcgmReturn_t GetNvSwitchList(unsigned int &count, unsigned int *switchIds, int64_t flags);

    /*************************************************************************/
    /**
     * Create the requested number of fake switch ids
     *
     * @param count[in/out]    - passed in as the request number of switch ids to create. Set to the number of fake
     *                           switch ids created.
     * @param switchIds[out]   - populated with the ids of the fake switches created here
     * @return DCGM_ST_OK:            all requested fake switches were created.
     *         DCGM_ST_GENERIC_ERROR: we couldn't create the requested number of fake switches
     */
    dcgmReturn_t CreateFakeSwitches(unsigned int &count, unsigned int *switchIds);

    /*************************************************************************/
    /**
     * Set field watch for NVSWITCH
     *
     * @param entityGroupId[in]       - Type of entity targeted. Must be DCGM_FE_SWITCH
     * @param entityId[in]            - Entity ID of targeted switch
     * @param numFieldIds[in]         - The number of field IDs in the fieldIds buffer below
     * @param fieldIds[in]            - Field IDs to watch
     * @param updateIntervalUsec[in]  - Interval between updates in microseconds
     * @param watcherType[in]         - the type of watcher
     * @param connectionId[in]        - the connection id, if any
     * @param forceWatch[in]          - whether we should force the fields to be watched, even
     *                                  if entityId is fake. true=yes (unit tests). false=no (production)
     *
     * @return DCGM_ST_OK:            Field watches successfully set
     *         DCGM_ST_BADPARAM:      Bad input parameter. Check logs for details
     */
    dcgmReturn_t WatchField(dcgm_field_entity_group_t entityGroupId,
                            unsigned int entityId,
                            unsigned int numFieldIds,
                            const unsigned short *fieldIds,
                            timelib64_t updateIntervalUsec,
                            DcgmWatcherType_t watcherType,
                            dcgm_connection_id_t connectionId,
                            bool forceWatch);

    /*************************************************************************/
    /**
     * Unset field watch for NVSWITCH
     *
     * @param watcherType[in]  - the type of watcher
     * @param connectionId[in] - the connection id, if any
     *
     * @return DCGM_ST_OK:            Field watches successfully unset
     *         DCGM_ST_*:             Indicating any other return
     */
    dcgmReturn_t UnwatchField(DcgmWatcherType_t watcherType, dcgm_connection_id_t connectionId);

    /*************************************************************************/
    /**
     * Get NvLink link states for a NvSwitch
     *
     * @param msg[in/out]  - Module command to process
     *
     * @return DCGM_ST_OK:            Link state successfully populated in msg
     *         DCGM_ST_*:             Indicating any other return
     */
    dcgmReturn_t GetLinkStates(dcgm_nvswitch_msg_get_link_states_t *msg);

    /*************************************************************************/
    /**
     * Get NvLink link states for every NvSwitch we know about
     *
     * @param msg[in/out]  - Module command to process
     *
     * @return DCGM_ST_OK:            Link state successfully populated in msg
     *         DCGM_ST_*:             Indicating any other return
     */
    dcgmReturn_t GetAllLinkStates(dcgm_nvswitch_msg_get_all_link_states_t *msg);

    /*************************************************************************/
    /**
     * Updates the fatal error fields in CacheManager
     *
     * @return DCGM_ST_OK:            Fields pushed correctly to the cache
     *         DCGM_ST_*:             Indicating any other return
     */
    dcgmReturn_t UpdateFatalErrorsAllSwitches();

    /*************************************************************************/
    /**
     * Updates the watched fields for this module
     *
     * @param nextUpdateTime[out]  - the earliest next time we should wake up
     *                               to update the fields
     *
     * @return DCGM_ST_OK:            Fields pushed correctly to the cache
     *         DCGM_ST_*:             Indicating any other return
     */
    dcgmReturn_t UpdateFields(timelib64_t &nextUpdateTime);

    /*************************************************************************/
    /*
     * Process a dcgm_nvswitch_msg_set_link_state_t message.
     */
    dcgmReturn_t SetEntityNvLinkLinkState(dcgm_nvswitch_msg_set_link_state_t *msg);

    /*************************************************************************/
    /*
     * Process a dcgm_nvswitch_msg_get_entity_status_t message.
     */
    dcgmReturn_t GetEntityStatus(dcgm_nvswitch_msg_get_entity_status_t *msg);

    /*************************************************************************/
    /**
     * Read switch status for all switches and update their information
     */
    dcgmReturn_t ReadNvSwitchStatusAllSwitches();

    /*************************************************************************/
    /**
     * Initialize Switch Manager
     *
     * Calling this method creates an NSCQ session for communicating with
     * switches and NVLINKs
     *
     *
     * @return DCGM_ST_OK:            Successfully initialized
     *         DCGM_ST_BADPARAM:      Already initialized
     *         DCGM_ST_NVML_ERROR:    NSCQ returned an error during session creation
     */
    dcgmReturn_t Init();

    /**
     * @brief Pauses the Switch Manager.
     *
     * When paused, the Switch Manager will detach from NSCQ and not update any fields.
     * The paused state is not considered as uninitialized and any update function will return \c DCGM_ST_STALE_DATA
     * instead of \c DCGM_ST_UNINITIALIZED.
     *
     *
     * @return \c DCGM_ST_OK:            Successfully paused
     * @return \c DCGM_ST_UNINITIALIZED: Switch Manager is not initialized
     */
    dcgmReturn_t Pause();
    /**
     * @brief Resumes the Switch Manager.
     *
     * When resumed, the Switch Manager will reattach to NSCQ.
     *
     *
     * @return \c DCGM_ST_OK:           Successfully resumed
     * @return Error code similar to \c Init() otherwise
     */
    dcgmReturn_t Resume();

protected:
    unsigned int m_numNvSwitches;                             // Number of entries in m_nvSwitches that are valid
    dcgm_nvswitch_info_t m_nvSwitches[DCGM_MAX_NUM_SWITCHES]; // All of the NvSwitches we know about
    uuid_p m_nvSwitchNscqDevices[DCGM_MAX_NUM_SWITCHES];      // Pointers to NSCQ device objects
    label_t m_nvSwitchUuids[DCGM_MAX_NUM_SWITCHES];           // UUID labels associated with each NvSwitch
    DcgmWatchTable m_watchTable;                              // Our internal watch table
    DcgmCoreProxy m_coreProxy;                                // Proxy class for communication with DCGM core
    nscq_session_t m_nscqSession;                             // NSCQ session for communicating with driver
    bool m_attachedToNscq = false;                            // Have we attached to nscq yet? */
    DcgmNvSwitchError m_fatalErrors[DCGM_MAX_NUM_SWITCHES];   // Fatal errors. Max 1 per switch
    bool m_paused = false;                                    // Is the Switch Manager paused?

    /*************************************************************************/
    /**
     * Adds one fake nv switch and returns the id, or returns DCGM_ENTITY_ID_BAD to signify failure
     */
    unsigned int AddFakeNvSwitch();

    /*************************************************************************/
    /**
     * Returns true if the specified id is present on the host, false otherwise
     */
    bool IsValidNvSwitchId(dcgm_field_eid_t entityId);

    /*************************************************************************/
    /**
     * Returns a pointer to the internal NvSwitch object by looking up its
     * physical ID (entityId). Returns nullptr on error.
     *
     * For links (entityGroupId==DCGM_FE_LINK),
     *
     * @return  Pointer to the internal NvSwitch object on success
     *          nullptr on error
     */
    dcgm_nvswitch_info_t *GetNvSwitchObject(dcgm_field_entity_group_t entityGroupId, dcgm_field_eid_t entityId);

    /*************************************************************************/
    /**
     * Scan the NSCQ driver for NvSwitches and add any new switches to m_nvSwitches.
     * This also sets the link state for each port of the nvSwitch.
     */
    dcgmReturn_t AttachNvSwitches();

    /*************************************************************************/
    /**
     * Returns index of switch with physical id provided, -1 if not found
     */
    int FindSwitchByPhysId(phys_id_t);

    /*************************************************************************/
    /**
     * Returns index of switch with NSCQ device provided, -1 if not found
     */
    int FindSwitchByDevice(uuid_p);

    /*************************************************************************/
    /**
     * Read link status for each link belonging to any switch and update switch
     * information accordingly
     */
    dcgmReturn_t ReadLinkStatesAllSwitches();

    /*************************************************************************/
    /**
     * Sets link state for single link. Does not query for data
     */
    dcgmReturn_t UpdateLinkState(unsigned int, link_id_t, nvlink_state_t);

    /*************************************************************************/
    /**
     * Read fatal errors for all switches
     */
    dcgmReturn_t ReadNvSwitchFatalErrorsAllSwitches();

    /*************************************************************************/
    /**
     * Helper to buffer blank values for all affected entities of a fieldId
     */
    void BufferBlankValueForAllEntities(unsigned short fieldId,
                                        DcgmFvBuffer &buf,
                                        const std::vector<dcgm_field_update_info_t> &entities);

    enum class ConnectionStatus
    {
        Ok,           //!< The NvSwitch Module is connected to the NSCQ
        Disconnected, //!< The NvSwitch Module is not connected to the NSCQ (not initialized)
        Paused,       //!< The NvSwitch Module is paused and not connected to the NSCQ
    };

    /**
     * @brief Returns the connection status of the Switch Manager.
     * @return \c ConnectionStatus::Ok:           The Switch Manager is connected to the NSCQ
     * @return \c ConnectionStatus::Disconnected: The Switch Manager is not connected to the NSCQ
     * @return \c ConnectionStatus::Paused:       The Switch Manager is paused and not connected to the NSCQ
     */
    ConnectionStatus CheckConnectionStatus() const;

public:
    /*************************************************************************/
    /*
     * Here we define Index lookup functions to check if an index matches
     * any of the supplied entities. The index is a tuple composed of the
     * various indices provided in an NSCQ lambda callback, and it is expected
     * this function will be fully specialized for each index case of multiple
     * indices (switch, link, lane, etc.)
     */
    template <typename... indexTypes>
    std::optional<dcgmGroupEntityPair_t> Find(unsigned short fieldId,
                                              const std::vector<dcgm_field_update_info_t> &entities,
                                              std::tuple<indexTypes...> index)
    {
        return std::nullopt;
    }

    /*************************************************************************/
    /**
     * Update Fields.
     */
    template <typename nscqFieldType, typename storageType, bool is_vector, typename... indexTypes>
    dcgmReturn_t UpdateFields(unsigned short fieldId,
                              DcgmFvBuffer &buf,
                              const std::vector<dcgm_field_update_info_t> &entities,
                              timelib64_t now);
};

template <>
std::optional<dcgmGroupEntityPair_t> DcgmNvSwitchManager::Find(unsigned short fieldId,
                                                               const std::vector<dcgm_field_update_info_t> &entities,
                                                               std::tuple<uuid_p> index);

template <>
std::optional<dcgmGroupEntityPair_t> DcgmNvSwitchManager::Find(unsigned short fieldId,
                                                               const std::vector<dcgm_field_update_info_t> &entities,
                                                               std::tuple<uuid_p, link_id_t> index);

template <>
std::optional<dcgmGroupEntityPair_t> DcgmNvSwitchManager::Find(unsigned short fieldId,
                                                               const std::vector<dcgm_field_update_info_t> &entities,
                                                               std::tuple<uuid_p, link_id_t, lane_vc_id_t> index);
} // namespace DcgmNs
