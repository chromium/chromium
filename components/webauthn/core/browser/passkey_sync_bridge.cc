// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_sync_bridge.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace webauthn {
namespace {

// The byte length of the WebauthnCredentialSpecifics `sync_id` field.
constexpr size_t kSyncIdLength = 16u;

// The byte length of the WebauthnCredentialSpecifics `credential_id` field.
constexpr size_t kCredentialIdLength = 16u;

// The maximum byte length of the WebauthnCredentialSpecifics `user_id` field.
constexpr size_t kUserIdMaxLength = 64u;

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::WebauthnCredentialSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  // Name must be UTF-8 decodable.
  entity_data->name =
      base::HexEncode(base::as_bytes(base::make_span(specifics.sync_id())));
  *entity_data->specifics.mutable_webauthn_credential() = specifics;
  return entity_data;
}

bool WebauthnCredentialSpecificsValid(
    const sync_pb::WebauthnCredentialSpecifics& specifics) {
  return specifics.sync_id().size() == kSyncIdLength &&
         specifics.credential_id().size() == kCredentialIdLength &&
         !specifics.rp_id().empty() &&
         specifics.user_id().length() <= kUserIdMaxLength &&
         (specifics.has_private_key() || specifics.has_encrypted());
}

std::optional<std::string> FindHeadOfShadowChain(
    const std::map<std::string, sync_pb::WebauthnCredentialSpecifics>& passkeys,
    const std::string& rp_id,
    const std::string& user_id) {
  // Collect all credentials for the user.id, rpid pair.
  std::vector<sync_pb::WebauthnCredentialSpecifics> rpid_passkeys;
  for (const auto& passkey : passkeys) {
    if (passkey.second.user_id() == user_id &&
        passkey.second.rp_id() == rp_id) {
      rpid_passkeys.emplace_back(passkey.second);
    }
  }
  // Filter the shadowed credentials.
  std::vector<sync_pb::WebauthnCredentialSpecifics> filtered =
      passkey_model_utils::FilterShadowedCredentials(rpid_passkeys);
  CHECK_LE(filtered.size(), 1u);
  return filtered.empty() ? std::nullopt
                          : std::make_optional(filtered.at(0).sync_id());
}

PasskeyModelChange::ChangeType ToPasskeyModelChangeType(
    syncer::EntityChange::ChangeType entity_change) {
  switch (entity_change) {
    case syncer::EntityChange::ACTION_ADD:
      return PasskeyModelChange::ChangeType::ADD;
    case syncer::EntityChange::ACTION_UPDATE:
      return PasskeyModelChange::ChangeType::UPDATE;
    case syncer::EntityChange::ACTION_DELETE:
      return PasskeyModelChange::ChangeType::REMOVE;
  }
}

}  // namespace

PasskeySyncBridge::PasskeySyncBridge(
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::WEBAUTHN_CREDENTIAL,
              /*dump_stack=*/base::DoNothing())) {
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  std::move(store_factory)
      .Run(syncer::WEBAUTHN_CREDENTIAL,
           base::BindOnce(&PasskeySyncBridge::OnCreateStore,
                          weak_ptr_factory_.GetWeakPtr()));
}

PasskeySyncBridge::~PasskeySyncBridge() {
  for (auto& observer : observers_) {
    observer.OnPasskeyModelShuttingDown();
  }
}

void PasskeySyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasskeySyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<syncer::MetadataChangeList>
PasskeySyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> PasskeySyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_changes,
    syncer::EntityChangeList entity_changes) {
  CHECK(base::ranges::all_of(entity_changes, [](const auto& change) {
    return change->type() == syncer::EntityChange::ACTION_ADD;
  }));

  // Google Password Manager passkeys are disabled when Sync is disabled so it
  // shouldn't be the case that there are any local entities when Sync starts.
  // But it can happen in corner cases. This code uploads any such entities to
  // the server.
  base::flat_set<std::string_view> local_only_sync_ids;
  for (const auto& it : data_) {
    local_only_sync_ids.insert(it.first);
  }
  for (const auto& change : entity_changes) {
    local_only_sync_ids.erase(change->storage_key());
  }
  for (const auto& local_only_sync_id : local_only_sync_ids) {
    std::string sync_id(local_only_sync_id);
    change_processor()->Put(sync_id, CreateEntityData(data_.at(sync_id)),
                            metadata_changes.get());
  }

  return ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
PasskeySyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  std::vector<PasskeyModelChange> changes;
  for (const auto& entity_change : entity_changes) {
    PasskeyModelChange::ChangeType change_type =
        ToPasskeyModelChangeType(entity_change->type());
    switch (entity_change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        const auto passkey_it = data_.find(entity_change->storage_key());
        if (passkey_it != data_.end()) {
          changes.emplace_back(change_type, passkey_it->second);
          data_.erase(passkey_it);
        } else {
          DVLOG(1) << "Downloaded deletion for passkey not present locally";
        }
        write_batch->DeleteData(entity_change->storage_key());
        break;
      }
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        // No merging is done and remote changes override local changes.
        const sync_pb::WebauthnCredentialSpecifics& specifics =
            entity_change->data().specifics.webauthn_credential();
        changes.emplace_back(change_type, specifics);
        data_[entity_change->storage_key()] = specifics;
        write_batch->WriteData(entity_change->storage_key(),
                               specifics.SerializeAsString());
        break;
      }
    }
  }

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  if (!entity_changes.empty()) {
    NotifyPasskeysChanged(std::move(changes));
  }
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> PasskeySyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& sync_id : storage_keys) {
    if (auto it = data_.find(sync_id); it != data_.end()) {
      batch->Put(sync_id, CreateEntityData(it->second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch> PasskeySyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [sync_id, specifics] : data_) {
    batch->Put(sync_id, CreateEntityData(specifics));
  }
  return batch;
}

bool PasskeySyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return WebauthnCredentialSpecificsValid(
      entity_data.specifics.webauthn_credential());
}

std::string PasskeySyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string PasskeySyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_webauthn_credential());
  return entity_data.specifics.webauthn_credential().sync_id();
}

void PasskeySyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  CHECK(store_);
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  std::vector<PasskeyModelChange> changes;
  for (const auto& passkey : data_) {
    changes.emplace_back(PasskeyModelChange::ChangeType::REMOVE,
                         passkey.second);
  }
  data_.clear();
  NotifyPasskeysChanged(std::move(changes));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
PasskeySyncBridge::GetDataTypeControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

bool PasskeySyncBridge::IsReady() const {
  return ready_;
}

bool PasskeySyncBridge::IsEmpty() const {
  return data_.empty();
}

base::flat_set<std::string> PasskeySyncBridge::GetAllSyncIds() const {
  std::vector<std::string> sync_ids;
  base::ranges::transform(data_, std::back_inserter(sync_ids),
                          [](const auto& pair) { return pair.first; });
  return base::flat_set<std::string>(base::sorted_unique, std::move(sync_ids));
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
PasskeySyncBridge::GetAllPasskeys() const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  base::ranges::transform(data_, std::back_inserter(passkeys),
                          [](const auto& pair) { return pair.second; });
  return passkeys;
}

std::optional<sync_pb::WebauthnCredentialSpecifics>
PasskeySyncBridge::GetPasskeyByCredentialId(
    const std::string& rp_id,
    const std::string& credential_id) const {
  // Even if a passkey with a credential ID exists, we must not return it if it
  // has been shadowed. To do that, first collect all passkeys for the RP ID,
  // then filter shadowed ones, and see if one with the matching credential ID
  // remains.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  for (const auto& passkey : data_) {
    if (passkey.second.rp_id() == rp_id) {
      passkeys.emplace_back(passkey.second);
    }
  }
  passkeys = passkey_model_utils::FilterShadowedCredentials(passkeys);
  for (const auto& passkey : passkeys) {
    if (passkey.credential_id() == credential_id) {
      return passkey;
    }
  }
  return std::nullopt;
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
PasskeySyncBridge::GetPasskeysForRelyingPartyId(
    const std::string& rp_id) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  for (const auto& passkey : data_) {
    if (passkey.second.rp_id() == rp_id) {
      passkeys.emplace_back(passkey.second);
    }
  }
  return passkey_model_utils::FilterShadowedCredentials(passkeys);
}

bool PasskeySyncBridge::DeletePasskey(const std::string& credential_id,
                                      const base::Location& location) {
  // Find the credential with the given |credential_id|.
  const auto passkey_it =
      base::ranges::find_if(data_, [&credential_id](const auto& passkey) {
        return passkey.second.credential_id() == credential_id;
      });
  if (passkey_it == data_.end()) {
    DVLOG(1) << "Attempted to delete non existent passkey";
    return false;
  }
  std::string rp_id = passkey_it->second.rp_id();
  std::string user_id = passkey_it->second.user_id();
  std::optional<std::string> shadow_head_sync_id =
      FindHeadOfShadowChain(data_, rp_id, user_id);

  // There must be a head of the shadow chain. Otherwise, something is wrong
  // with the data. Bail out.
  if (!shadow_head_sync_id) {
    DVLOG(1) << "Could not find head of shadow chain";
    return false;
  }

  base::flat_set<std::string> sync_ids_to_delete;
  if (passkey_it->first == *shadow_head_sync_id) {
    // Remove all credentials for the user.id and rpid.
    for (const auto& passkey : data_) {
      if (passkey.second.rp_id() == rp_id &&
          passkey.second.user_id() == user_id) {
        sync_ids_to_delete.emplace(passkey.first);
      }
    }
  } else {
    // Remove only the passed credential.
    sync_ids_to_delete.emplace(passkey_it->first);
  }
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  std::vector<PasskeyModelChange> changes;
  for (const std::string& sync_id : sync_ids_to_delete) {
    changes.emplace_back(PasskeyModelChange::ChangeType::REMOVE,
                         data_.at(sync_id));
    data_.erase(sync_id);
    change_processor()->Delete(sync_id,
                               syncer::DeletionOrigin::FromLocation(location),
                               write_batch->GetMetadataChangeList());
    write_batch->DeleteData(sync_id);
  }
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyPasskeysChanged(std::move(changes));
  return true;
}

// The following implementation is more efficient than the simple one which
// would iterate over all passkeys and delete them one by one.
// Deleting all passkeys individually would also send out a notification to
// the observers for each individual deletion. This implementation only sends
// out a single notification for all deletions.
// Shadow chains are not handled separately since all passkeys are deleted
// anyway.
void PasskeySyncBridge::DeleteAllPasskeys() {
  CHECK(IsReady());

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  std::vector<PasskeyModelChange> changes;
  for (const auto& [sync_id, passkey] : data_) {
    changes.emplace_back(PasskeyModelChange::ChangeType::REMOVE,
                         std::move(passkey));
    change_processor()->Delete(sync_id,
                               syncer::DeletionOrigin::FromLocation(FROM_HERE),
                               write_batch->GetMetadataChangeList());
    write_batch->DeleteData(sync_id);
  }
  data_.clear();
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));

  // Sends out only a single notification for all deleted passkeys.
  NotifyPasskeysChanged(std::move(changes));
}

bool PasskeySyncBridge::UpdatePasskey(const std::string& credential_id,
                                      PasskeyUpdate change,
                                      bool updated_by_user) {
  // Find the credential with the given |credential_id|.
  const auto passkey_it =
      base::ranges::find_if(data_, [&credential_id](const auto& passkey) {
        return passkey.second.credential_id() == credential_id;
      });
  if (passkey_it == data_.end()) {
    DVLOG(1) << "Attempted to update non existent passkey";
    return false;
  }
  if (passkey_it->second.edited_by_user() && !updated_by_user) {
    // Respect the user's choice and do not change a passkey's user data if
    // explicitly set by the user previously.
    return false;
  }
  passkey_it->second.set_edited_by_user(updated_by_user);
  passkey_it->second.set_user_name(std::move(change.user_name));
  passkey_it->second.set_user_display_name(std::move(change.user_display_name));
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  change_processor()->Put(passkey_it->second.sync_id(),
                          CreateEntityData(passkey_it->second),
                          write_batch->GetMetadataChangeList());
  write_batch->WriteData(passkey_it->second.sync_id(),
                         passkey_it->second.SerializeAsString());
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyPasskeysChanged({PasskeyModelChange(
      PasskeyModelChange::ChangeType::UPDATE, passkey_it->second)});
  return true;
}

bool PasskeySyncBridge::UpdatePasskeyTimestamp(const std::string& credential_id,
                                               base::Time last_used_time) {
  const auto passkey_it =
      base::ranges::find_if(data_, [&credential_id](const auto& passkey) {
        return passkey.second.credential_id() == credential_id;
      });
  if (passkey_it == data_.end()) {
    DVLOG(1) << "Attempted to update non existent passkey";
    return false;
  }

  passkey_it->second.set_last_used_time_windows_epoch_micros(
      last_used_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  change_processor()->Put(passkey_it->second.sync_id(),
                          CreateEntityData(passkey_it->second),
                          write_batch->GetMetadataChangeList());
  write_batch->WriteData(passkey_it->second.sync_id(),
                         passkey_it->second.SerializeAsString());
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyPasskeysChanged({PasskeyModelChange(
      PasskeyModelChange::ChangeType::UPDATE, passkey_it->second)});
  return true;
}

sync_pb::WebauthnCredentialSpecifics PasskeySyncBridge::CreatePasskey(
    std::string_view rp_id,
    const UserEntity& user_entity,
    base::span<const uint8_t> trusted_vault_key,
    int32_t trusted_vault_key_version,
    std::vector<uint8_t>* public_key_spki_der_out) {
  CHECK(IsReady());

  auto [specifics, public_key_spki_der] =
      webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          rp_id, user_entity, trusted_vault_key, trusted_vault_key_version);

  AddShadowedCredentialIdsToNewPasskey(specifics);

  AddPasskeyInternal(specifics);

  if (public_key_spki_der_out != nullptr) {
    *public_key_spki_der_out = std::move(public_key_spki_der);
  }
  return specifics;
}

void PasskeySyncBridge::CreatePasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  // TODO(crbug.com/349547003): make it sure that all the callers check for
  // that. If not, it's still safer to crash in this case to avoid losing the
  // passkey.
  CHECK(IsReady());

  CHECK(WebauthnCredentialSpecificsValid(passkey));

  std::string sync_id = passkey.sync_id();
  CHECK(!base::Contains(data_, sync_id));

  AddShadowedCredentialIdsToNewPasskey(passkey);
  AddPasskeyInternal(passkey);
}

std::string PasskeySyncBridge::AddNewPasskeyForTesting(
    sync_pb::WebauthnCredentialSpecifics specifics) {
  const std::string sync_id = specifics.sync_id();
  AddPasskeyInternal(std::move(specifics));
  return sync_id;
}

void PasskeySyncBridge::AddPasskeyInternal(
    sync_pb::WebauthnCredentialSpecifics specifics) {
  CHECK(WebauthnCredentialSpecificsValid(specifics));
  CHECK(IsReady());
  CHECK(store_);

  std::string sync_id = specifics.sync_id();
  CHECK(!base::Contains(data_, sync_id));

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  change_processor()->Put(sync_id, CreateEntityData(specifics),
                          write_batch->GetMetadataChangeList());
  write_batch->WriteData(sync_id, specifics.SerializeAsString());
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  data_[sync_id] = specifics;
  NotifyPasskeysChanged({PasskeyModelChange(PasskeyModelChange::ChangeType::ADD,
                                            std::move(specifics))});
}

void PasskeySyncBridge::OnCreateStore(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  DCHECK(store);
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&PasskeySyncBridge::OnStoreReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasskeySyncBridge::OnStoreReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "PasskeySyncBridge::OnStoreReadAllDataAndMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    // Notify observers that the model failed to become ready.
    NotifyPasskeyModelIsReady(ready_);
    return;
  }

  std::vector<PasskeyModelChange> changes;
  for (const syncer::DataTypeStore::Record& r : *entries) {
    sync_pb::WebauthnCredentialSpecifics specifics;
    if (!specifics.ParseFromString(r.value) || !specifics.has_sync_id()) {
      DVLOG(1) << "Invalid stored record: " << r.value;
      continue;
    }
    std::string storage_key = specifics.sync_id();
    changes.emplace_back(PasskeyModelChange::ChangeType::ADD, specifics);
    data_[std::move(storage_key)] = std::move(specifics);
  }
  ready_ = true;
  NotifyPasskeysChanged(std::move(changes));
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  NotifyPasskeyModelIsReady(ready_);
}

void PasskeySyncBridge::OnStoreCommitWriteBatch(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
}

void PasskeySyncBridge::NotifyPasskeysChanged(
    const std::vector<PasskeyModelChange>& changes) {
  TRACE_EVENT0("sync", "PasskeySyncBridge::NotifyPasskeysChanged");
  for (auto& observer : observers_) {
    observer.OnPasskeysChanged(changes);
  }
}

void PasskeySyncBridge::NotifyPasskeyModelIsReady(bool is_ready) {
  TRACE_EVENT0("sync", "PasskeySyncBridge::NotifyPasskeyModelIsReady");
  for (auto& observer : observers_) {
    observer.OnPasskeyModelIsReady(is_ready);
  }
}

void PasskeySyncBridge::AddShadowedCredentialIdsToNewPasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  for (const auto& [sync_id, existing_passkey] : data_) {
    if (passkey.rp_id() == existing_passkey.rp_id() &&
        passkey.user_id() == existing_passkey.user_id()) {
      passkey.add_newly_shadowed_credential_ids(
          existing_passkey.credential_id());
    }
  }
}

}  // namespace webauthn
