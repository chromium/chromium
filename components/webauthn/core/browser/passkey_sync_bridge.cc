// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_sync_bridge.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

PasskeySyncBridge::PasskeySyncBridge(
    syncer::OnceModelTypeStoreFactory store_factory)
    : syncer::ModelTypeSyncBridge(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEBAUTHN_CREDENTIAL,
              /*dump_stack=*/base::DoNothing())) {
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  NOTIMPLEMENTED();  // Run `store_factory`.
}

PasskeySyncBridge::~PasskeySyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PasskeySyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> PasskeySyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_changes,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<syncer::ModelError> PasskeySyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void PasskeySyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  NOTIMPLEMENTED();
}

void PasskeySyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
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

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasskeySyncBridge::GetModelTypeControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}
