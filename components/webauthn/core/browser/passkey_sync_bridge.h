// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace syncer {
struct EntityData;
class MetadataChangeList;
class ModelError;
}  // namespace syncer

// Sync bridge implementation for WEBAUTHN_CREDENTIAL model type.
class PasskeySyncBridge : public syncer::ModelTypeSyncBridge,
                          public PasskeyModel {
 public:
  explicit PasskeySyncBridge(syncer::OnceModelTypeStoreFactory store_factory);
  PasskeySyncBridge(const PasskeySyncBridge&) = delete;
  PasskeySyncBridge& operator=(const PasskeySyncBridge&) = delete;
  ~PasskeySyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // PasskeyModel:
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetModelTypeControllerDelegate() override;
};

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_
