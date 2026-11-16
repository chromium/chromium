// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/fake_gcm_store.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"

namespace gcm {

FakeGCMStore::FakeGCMStore() = default;

FakeGCMStore::~FakeGCMStore() = default;

void FakeGCMStore::Load(StoreOpenMode open_mode, LoadCallback callback) {
  auto result = std::make_unique<LoadResult>();
  result->success = true;
  result->registrations = registrations_;
  result->instance_id_data = instance_id_data_;
  std::move(callback).Run(std::move(result));
}

void FakeGCMStore::Close() {}

void FakeGCMStore::Destroy(UpdateCallback callback) {
  registrations_.clear();
  instance_id_data_.clear();
  std::move(callback).Run(true);
}

void FakeGCMStore::SetDeviceCredentials(uint64_t device_android_id,
                                        uint64_t device_security_token,
                                        UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::AddRegistration(const std::string& serialized_key,
                                   const std::string& serialized_value,
                                   UpdateCallback callback) {
  registrations_[serialized_key] = serialized_value;
  std::move(callback).Run(true);
}

void FakeGCMStore::RemoveRegistration(const std::string& serialized_key,
                                      UpdateCallback callback) {
  registrations_.erase(serialized_key);
  std::move(callback).Run(true);
}

void FakeGCMStore::AddIncomingMessage(const std::string& persistent_id,
                                      UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::RemoveIncomingMessages(
    const PersistentIdList& persistent_ids,
    UpdateCallback callback) {
  std::move(callback).Run(true);
}

bool FakeGCMStore::AddOutgoingMessage(const std::string& persistent_id,
                                      const MCSMessage& message,
                                      UpdateCallback callback) {
  std::move(callback).Run(true);
  return true;
}

void FakeGCMStore::OverwriteOutgoingMessage(const std::string& persistent_id,
                                            const MCSMessage& message,
                                            UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::RemoveOutgoingMessage(const std::string& persistent_id,
                                         UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::RemoveOutgoingMessages(
    const PersistentIdList& persistent_ids,
    UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::SetLastCheckinInfo(const base::Time& time,
                                      UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::SetGServicesSettings(
    const std::map<std::string, std::string>& settings,
    const std::string& settings_digest,
    UpdateCallback callback) {
  std::move(callback).Run(true);
}

void FakeGCMStore::AddInstanceIDData(const std::string& app_id,
                                     const std::string& instance_id_data,
                                     UpdateCallback callback) {
  instance_id_data_[app_id] = instance_id_data;
  std::move(callback).Run(true);
}

void FakeGCMStore::RemoveInstanceIDData(const std::string& app_id,
                                        UpdateCallback callback) {
  instance_id_data_.erase(app_id);
  std::move(callback).Run(true);
}

}  // namespace gcm
