// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_FAKE_GCM_STORE_H_
#define GOOGLE_APIS_GCM_ENGINE_FAKE_GCM_STORE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/time/time.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

class FakeGCMStore : public GCMStore {
 public:
  FakeGCMStore();

  FakeGCMStore(const FakeGCMStore&) = delete;
  FakeGCMStore& operator=(const FakeGCMStore&) = delete;

  ~FakeGCMStore() override;

  // GCMStore implementation.
  void Load(StoreOpenMode open_mode, LoadCallback callback) override;
  void Close() override;
  void Destroy(UpdateCallback callback) override;
  void SetDeviceCredentials(uint64_t device_android_id,
                            uint64_t device_security_token,
                            UpdateCallback callback) override;
  void AddRegistration(const std::string& serialized_key,
                       const std::string& serialized_value,
                       UpdateCallback callback) override;
  void RemoveRegistration(const std::string& serialized_key,
                          UpdateCallback callback) override;
  void AddIncomingMessage(const std::string& persistent_id,
                          UpdateCallback callback) override;
  void RemoveIncomingMessages(const PersistentIdList& persistent_ids,
                              UpdateCallback callback) override;
  bool AddOutgoingMessage(const std::string& persistent_id,
                          const MCSMessage& message,
                          UpdateCallback callback) override;
  void OverwriteOutgoingMessage(const std::string& persistent_id,
                                const MCSMessage& message,
                                UpdateCallback callback) override;
  void RemoveOutgoingMessage(const std::string& persistent_id,
                             UpdateCallback callback) override;
  void RemoveOutgoingMessages(const PersistentIdList& persistent_ids,
                              UpdateCallback callback) override;
  void SetLastCheckinInfo(const base::Time& time,
                          UpdateCallback callback) override;
  void SetGServicesSettings(const std::map<std::string, std::string>& settings,
                            const std::string& settings_digest,
                            UpdateCallback callback) override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id_data,
                         UpdateCallback callback) override;
  void RemoveInstanceIDData(const std::string& app_id,
                            UpdateCallback callback) override;

  const std::map<std::string, std::string>& registrations() const {
    return registrations_;
  }
  const std::map<std::string, std::string>& instance_id_data() const {
    return instance_id_data_;
  }

 private:
  std::map<std::string, std::string> registrations_;
  std::map<std::string, std::string> instance_id_data_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_FAKE_GCM_STORE_H_
