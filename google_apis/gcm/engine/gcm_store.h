// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_

#include <google/protobuf/message_lite.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/account_mapping.h"

namespace gcm {

class MCSMessage;

// A GCM data store interface. GCM Store will handle persistence portion of RMQ,
// as well as store device and user checkin information.
class GCM_EXPORT GCMStore {
 public:
  enum StoreOpenMode {
    DO_NOT_CREATE,
    CREATE_IF_MISSING
  };

  // Map of message id to message data for outgoing messages.
  using OutgoingMessageMap =
      std::map<std::string, std::unique_ptr<google::protobuf::MessageLite>>;

  // List of account mappings.
  using AccountMappings = std::vector<AccountMapping>;

  // Container for Load(..) results.
  struct GCM_EXPORT LoadResult {
    LoadResult();
    ~LoadResult();

    void Reset();

    bool success;
    bool store_does_not_exist;
    uint64_t device_android_id;
    uint64_t device_security_token;
    std::map<std::string, std::string> registrations;
    std::vector<std::string> incoming_messages;
    OutgoingMessageMap outgoing_messages;
    std::map<std::string, std::string> gservices_settings;
    std::string gservices_digest;
    base::Time last_checkin_time;
    std::set<std::string> last_checkin_accounts;
    AccountMappings account_mappings;
    base::Time last_token_fetch_time;
    std::map<std::string, int> heartbeat_intervals;
    std::map<std::string, std::string> instance_id_data;
  };

  using PersistentIdList = std::vector<std::string>;
  using LoadCallback =
      base::OnceCallback<void(std::unique_ptr<LoadResult> result)>;
  using UpdateCallback = base::OnceCallback<void(bool success)>;

  GCMStore();

  GCMStore(const GCMStore&) = delete;
  GCMStore& operator=(const GCMStore&) = delete;

  virtual ~GCMStore();

  // Load the data from persistent store and pass the initial state back to
  // caller.
  virtual void Load(StoreOpenMode open_mode, LoadCallback callback) = 0;

  // Close the persistent store.
  virtual void Close() = 0;

  // Clears the GCM store of all data.
  virtual void Destroy(UpdateCallback callback) = 0;

  // Sets this device's messaging credentials.
  virtual void SetDeviceCredentials(uint64_t device_android_id,
                                    uint64_t device_security_token,
                                    UpdateCallback callback) = 0;

  // Registration info for both GCM registrations and InstanceID tokens.
  // For GCM, |serialized_key| is app_id and |serialized_value| is
  // serialization of (senders, registration_id). For InstanceID,
  // |serialized_key| is serialization of (app_id, authorized_entity, scope)
  // and |serialized_value| is token.
  virtual void AddRegistration(const std::string& serialized_key,
                               const std::string& serialized_value,
                               UpdateCallback callback) = 0;
  virtual void RemoveRegistration(const std::string& serialized_key,
                                  UpdateCallback callback) = 0;

  // Unacknowledged incoming message handling.
  virtual void AddIncomingMessage(const std::string& persistent_id,
                                  UpdateCallback callback) = 0;
  virtual void RemoveIncomingMessage(const std::string& persistent_id,
                                     UpdateCallback callback) = 0;
  virtual void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                                      UpdateCallback callback) = 0;

  // Unacknowledged outgoing messages handling.
  // Returns false if app has surpassed message limits, else returns true. Note
  // that the message isn't persisted until |callback| is invoked with
  // |success| == true.
  virtual bool AddOutgoingMessage(const std::string& persistent_id,
                                  const MCSMessage& message,
                                  UpdateCallback callback) = 0;
  virtual void OverwriteOutgoingMessage(const std::string& persistent_id,
                                        const MCSMessage& message,
                                        UpdateCallback callback) = 0;
  virtual void RemoveOutgoingMessage(const std::string& persistent_id,
                                     UpdateCallback callback) = 0;
  virtual void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                                      UpdateCallback callback) = 0;

  // Sets last device's checkin information.
  virtual void SetLastCheckinInfo(const base::Time& time,
                                  const std::set<std::string>& accounts,
                                  UpdateCallback callback) = 0;

  // G-service settings handling.
  // Persists |settings| and |settings_digest|. It completely replaces the
  // existing data.
  virtual void SetGServicesSettings(
      const std::map<std::string, std::string>& settings,
      const std::string& settings_digest,
      UpdateCallback callback) = 0;

  // Sets the account information related to device to account mapping.
  virtual void AddAccountMapping(const AccountMapping& account_mapping,
                                 UpdateCallback callback) = 0;
  virtual void RemoveAccountMapping(const CoreAccountId& account_id,
                                    UpdateCallback callback) = 0;

  // Sets last token fetch time.
  virtual void SetLastTokenFetchTime(const base::Time& time,
                                     UpdateCallback callback) = 0;

  // Sets the custom client heartbeat interval for a specified scope.
  virtual void AddHeartbeatInterval(const std::string& scope,
                                    int interval_ms,
                                    UpdateCallback callback) = 0;
  virtual void RemoveHeartbeatInterval(const std::string& scope,
                                       UpdateCallback callback) = 0;

  // Instance ID data.
  virtual void AddInstanceIDData(const std::string& app_id,
                                 const std::string& instance_id_data,
                                 UpdateCallback callback) = 0;
  virtual void RemoveInstanceIDData(const std::string& app_id,
                                    UpdateCallback callback) = 0;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_STORE_H_
