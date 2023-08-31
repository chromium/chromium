// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_sync_bridge.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
         specifics.user_id().length() <= kUserIdMaxLength;
}

absl::optional<std::string> FindHeadOfShadowChain(
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
  return filtered.empty() ? absl::nullopt
                          : absl::make_optional(filtered.at(0).sync_id());
}

}  // namespace

PasskeySyncBridge::PasskeySyncBridge(
    syncer::OnceModelTypeStoreFactory store_factory)
    : syncer::ModelTypeSyncBridge(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEBAUTHN_CREDENTIAL,
              /*dump_stack=*/base::DoNothing())) {
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  std::move(store_factory)
      .Run(syncer::WEBAUTHN_CREDENTIAL,
           base::BindOnce(&PasskeySyncBridge::OnCreateStore,
                          weak_ptr_factory_.GetWeakPtr()));
}

PasskeySyncBridge::~PasskeySyncBridge() = default;

void PasskeySyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasskeySyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<syncer::MetadataChangeList>
PasskeySyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> PasskeySyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_changes,
    syncer::EntityChangeList entity_changes) {
  // Passkeys should be deleted when sync is turned off. Therefore, there should
  // be no local data at this point.
  CHECK(data_.empty());

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  // Merge sync to local data. Since there should be no local-only passkeys for
  // now, we don't actually need to merge anything yet. If we do merge, we need
  // to feed the changes back to `change_processor()`.
  for (const auto& entity_change : entity_changes) {
    const sync_pb::WebauthnCredentialSpecifics& specifics =
        entity_change->data().specifics.webauthn_credential();
    data_[entity_change->storage_key()] = specifics;
    write_batch->WriteData(entity_change->storage_key(),
                           specifics.SerializeAsString());
  }

  // No data is local-only for now. No need to write local entries back to sync.
  write_batch->TakeMetadataChangesFrom(std::move(metadata_changes));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyPasskeysChanged();
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
PasskeySyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();

  for (const auto& entity_change : entity_changes) {
    switch (entity_change->type()) {
      case syncer::EntityChange::ACTION_DELETE: {
        data_.erase(entity_change->storage_key());
        write_batch->DeleteData(entity_change->storage_key());
        break;
      }
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::WebauthnCredentialSpecifics& specifics =
            entity_change->data().specifics.webauthn_credential();
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
    NotifyPasskeysChanged();
  }
  return absl::nullopt;
}

void PasskeySyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& sync_id : storage_keys) {
    if (auto it = data_.find(sync_id); it != data_.end()) {
      batch->Put(sync_id, CreateEntityData(it->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void PasskeySyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [sync_id, specifics] : data_) {
    batch->Put(sync_id, CreateEntityData(specifics));
  }
  std::move(callback).Run(std::move(batch));
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
  data_.clear();
  NotifyPasskeysChanged();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasskeySyncBridge::GetModelTypeControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

base::flat_set<std::string> PasskeySyncBridge::GetAllSyncIds() const {
  std::vector<std::string> sync_ids;
  std::transform(data_.begin(), data_.end(), std::back_inserter(sync_ids),
                 [](const auto& pair) { return pair.first; });
  return base::flat_set<std::string>(base::sorted_unique, std::move(sync_ids));
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
PasskeySyncBridge::GetAllPasskeys() const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  std::transform(data_.begin(), data_.end(), std::back_inserter(passkeys),
                 [](const auto& pair) { return pair.second; });
  return passkeys;
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

bool PasskeySyncBridge::DeletePasskey(const std::string& credential_id) {
  // Find the credential with the given |credential_id|.
  const auto passkey_it =
      std::ranges::find_if(data_, [&credential_id](const auto& passkey) {
        return passkey.second.credential_id() == credential_id;
      });
  if (passkey_it == data_.end()) {
    DVLOG(1) << "Attempted to delete non existent passkey";
    return false;
  }
  std::string rp_id = passkey_it->second.rp_id();
  std::string user_id = passkey_it->second.user_id();
  absl::optional<std::string> shadow_head_sync_id =
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
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  for (const std::string& sync_id : sync_ids_to_delete) {
    data_.erase(sync_id);
    change_processor()->Delete(sync_id, write_batch->GetMetadataChangeList());
    write_batch->DeleteData(sync_id);
  }
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyPasskeysChanged();
  return true;
}

bool PasskeySyncBridge::UpdatePasskey(const std::string& credential_id,
                                      PasskeyChange change) {
  // Find the credential with the given |credential_id|.
  const auto passkey_it =
      std::ranges::find_if(data_, [&credential_id](const auto& passkey) {
        return passkey.second.credential_id() == credential_id;
      });
  if (passkey_it == data_.end()) {
    DVLOG(1) << "Attempted to update non existent passkey";
    return false;
  }
  passkey_it->second.set_user_name(std::move(change.user_name));
  passkey_it->second.set_user_display_name(std::move(change.user_display_name));
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
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
  NotifyPasskeysChanged();
  return true;
}

std::string PasskeySyncBridge::AddNewPasskeyForTesting(
    sync_pb::WebauthnCredentialSpecifics specifics) {
  CHECK(WebauthnCredentialSpecificsValid(specifics));

  const std::string& sync_id = specifics.sync_id();
  CHECK(!base::Contains(data_, sync_id));

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  change_processor()->Put(sync_id, CreateEntityData(specifics),
                          write_batch->GetMetadataChangeList());
  write_batch->WriteData(sync_id, specifics.SerializeAsString());
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&PasskeySyncBridge::OnStoreCommitWriteBatch,
                     weak_ptr_factory_.GetWeakPtr()));
  data_[sync_id] = std::move(specifics);
  NotifyPasskeysChanged();
  return sync_id;
}

void PasskeySyncBridge::OnCreateStore(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  DCHECK(store);
  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&PasskeySyncBridge::OnStoreReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void PasskeySyncBridge::OnStoreReadAllData(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_->ReadAllMetadata(
      base::BindOnce(&PasskeySyncBridge::OnStoreReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entries)));
}

void PasskeySyncBridge::OnStoreReadAllMetadata(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "PasskeySyncBridge::OnStoreReadAllMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::ModelTypeStore::Record& r : *entries) {
    sync_pb::WebauthnCredentialSpecifics specifics;
    if (!specifics.ParseFromString(r.value) || !specifics.has_sync_id()) {
      DVLOG(1) << "Invalid stored record: " << r.value;
      continue;
    }
    std::string storage_key = specifics.sync_id();
    data_[std::move(storage_key)] = std::move(specifics);
  }
  NotifyPasskeysChanged();
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void PasskeySyncBridge::OnStoreCommitWriteBatch(
    const absl::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
}

void PasskeySyncBridge::NotifyPasskeysChanged() {
  TRACE_EVENT0("sync", "PasskeySyncBridge::NotifyPasskeysChanged");
  for (auto& observer : observers_) {
    observer.OnPasskeysChanged();
  }
}

}  // namespace webauthn
