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
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace syncer {
struct EntityData;
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace webauthn {

// Sync bridge implementation for WEBAUTHN_CREDENTIAL data type.
class PasskeySyncBridge : public syncer::DataTypeSyncBridge,
                          public PasskeyModel {
 public:
  explicit PasskeySyncBridge(syncer::OnceDataTypeStoreFactory store_factory);
  PasskeySyncBridge(const PasskeySyncBridge&) = delete;
  PasskeySyncBridge& operator=(const PasskeySyncBridge&) = delete;
  ~PasskeySyncBridge() override;

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // PasskeyModel:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetDataTypeControllerDelegate() override;
  bool IsReady() const override;
  bool IsEmpty() const override;
  base::flat_set<std::string> GetAllSyncIds() const override;
  std::vector<sync_pb::WebauthnCredentialSpecifics> GetAllPasskeys()
      const override;
  std::optional<sync_pb::WebauthnCredentialSpecifics> GetPasskeyByCredentialId(
      const std::string& rp_id,
      const std::string& credential_id) const override;
  std::vector<sync_pb::WebauthnCredentialSpecifics>
  GetPasskeysForRelyingPartyId(const std::string& rp_id) const override;
  bool DeletePasskey(const std::string& credential_id,
                     const base::Location& location) override;
  void DeleteAllPasskeys() override;
  bool UpdatePasskey(const std::string& credential_id,
                     PasskeyUpdate change,
                     bool updated_by_user) override;
  bool UpdatePasskeyTimestamp(const std::string& credential_id,
                              base::Time last_used_time) override;
  sync_pb::WebauthnCredentialSpecifics CreatePasskey(
      std::string_view rp_id,
      const UserEntity& user_entity,
      base::span<const uint8_t> trusted_vault_key,
      int32_t trusted_vault_key_version,
      std::vector<uint8_t>* public_key_spki_der_out) override;
  void CreatePasskey(sync_pb::WebauthnCredentialSpecifics& passkey) override;
  std::string AddNewPasskeyForTesting(
      sync_pb::WebauthnCredentialSpecifics passkey) override;

 private:
  void OnCreateStore(const std::optional<syncer::ModelError>& error,
                     std::unique_ptr<syncer::DataTypeStore> store);
  void OnStoreReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnStoreCommitWriteBatch(const std::optional<syncer::ModelError>& error);
  void NotifyPasskeyModelIsReady(bool is_ready);
  void NotifyPasskeysChanged(const std::vector<PasskeyModelChange>& changes);
  void AddPasskeyInternal(sync_pb::WebauthnCredentialSpecifics specifics);
  void AddShadowedCredentialIdsToNewPasskey(
      sync_pb::WebauthnCredentialSpecifics& passkey);

  // Local view of the stored data. Indexes specifics protos by storage key.
  std::map<std::string, sync_pb::WebauthnCredentialSpecifics> data_;

  // Passkeys are stored locally in leveldb.
  std::unique_ptr<syncer::DataTypeStore> store_;

  base::ObserverList<Observer> observers_;

  // Set to true once `data_` has been loaded and the model is ready to sync.
  bool ready_ = false;

  base::WeakPtrFactory<PasskeySyncBridge> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_SYNC_BRIDGE_H_
