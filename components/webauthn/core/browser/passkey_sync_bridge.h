// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace syncer {
struct EntityData;
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace webauthn {

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
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // PasskeyModel:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetModelTypeControllerDelegate() override;
  base::flat_set<std::string> GetAllSyncIds() const override;
  std::vector<sync_pb::WebauthnCredentialSpecifics> GetAllPasskeys()
      const override;
  bool DeletePasskey(const std::string& credential_id) override;
  bool UpdatePasskey(const std::string& credential_id,
                     PasskeyChange change) override;
  std::string AddNewPasskeyForTesting(
      sync_pb::WebauthnCredentialSpecifics passkey) override;

 private:
  void OnCreateStore(const absl::optional<syncer::ModelError>& error,
                     std::unique_ptr<syncer::ModelTypeStore> store);
  void OnStoreReadAllData(
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);
  void OnStoreReadAllMetadata(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommitWriteBatch(const absl::optional<syncer::ModelError>& error);
  void NotifyPasskeysChanged();

  // Local view of the stored data. Indexes specifics protos by storage key.
  std::map<std::string, sync_pb::WebauthnCredentialSpecifics> data_;

  // Passkeys are stored locally in leveldb.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PasskeySyncBridge> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_
