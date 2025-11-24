// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/import/imported_passkey_checker.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace webauthn {
namespace {

ImportedPasskeyInfo SpecificsToImportedPasskeyInfo(
    const sync_pb::WebauthnCredentialSpecifics& specifics,
    ImportedPasskeyStatus status) {
  return {.rp_id = specifics.rp_id(),
          .user_name = specifics.user_name(),
          .status = status};
}

}  // namespace

PasskeyImporter::PasskeyImporter(PasskeyModel& passkey_model)
    : passkey_model_(passkey_model) {}

PasskeyImporter::~PasskeyImporter() = default;

void PasskeyImporter::StartImport(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    ProcessingCallback processing_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasskeyImporter::ProcessPasskeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(passkeys),
                     std::move(processing_callback)));
}

void PasskeyImporter::FinishImport(
    std::vector<int> selected_conflicting_passkey_ids,
    base::OnceCallback<void(int)> passkeys_imported_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PasskeyImporter::ImportPasskeys,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(selected_conflicting_passkey_ids),
                                std::move(passkeys_imported_callback)));
}

void PasskeyImporter::ProcessPasskeys(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    ProcessingCallback processing_callback) {
  ImportProcessingResult result;
  for (sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    ImportedPasskeyStatus status = CheckImportedPasskey(passkey);
    if (status != ImportedPasskeyStatus::kOk) {
      result.errors.push_back(SpecificsToImportedPasskeyInfo(passkey, status));
      continue;
    }

    // TODO(crbug.com/458337350): Sanity check only matching credential ID.
    if (passkey_model_->GetPasskeyByUserId(passkey.rp_id(), passkey.user_id())
            .has_value()) {
      result.conflicts.push_back(
          SpecificsToImportedPasskeyInfo(passkey, status));
      conflicting_passkeys_.push_back(std::move(passkey));
      continue;
    }

    valid_passkeys_.push_back(std::move(passkey));
    result.valid_passkeys_amount++;
  }
  std::move(processing_callback).Run(result);
}

void PasskeyImporter::ImportPasskeys(
    std::vector<int> selected_conflicting_passkey_ids,
    base::OnceCallback<void(int)> passkeys_imported_callback) {
  for (sync_pb::WebauthnCredentialSpecifics& passkey : valid_passkeys_) {
    passkey_model_->CreatePasskey(passkey);
  }

  size_t conflicting_passkey_cache_size = conflicting_passkeys_.size();
  for (int incoming_passkey_id : selected_conflicting_passkey_ids) {
    CHECK_LT(static_cast<size_t>(incoming_passkey_id),
             conflicting_passkey_cache_size);
    passkey_model_->CreatePasskey(conflicting_passkeys_[incoming_passkey_id]);
  }

  size_t imported_passkeys_count =
      valid_passkeys_.size() + selected_conflicting_passkey_ids.size();
  std::move(passkeys_imported_callback)
      .Run(static_cast<int>(imported_passkeys_count));
}

}  // namespace webauthn
