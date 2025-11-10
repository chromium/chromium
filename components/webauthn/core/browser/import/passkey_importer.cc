// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace webauthn {
namespace {

ImportedPasskeyInfo SpecificsToImportedPasskeyInfo(
    const sync_pb::WebauthnCredentialSpecifics& specifics) {
  return {.rp_id = specifics.rp_id(), .user_name = specifics.user_name()};
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

void PasskeyImporter::ProcessPasskeys(
    std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
    ProcessingCallback processing_callback) {
  ImportProcessingResult result;
  for (sync_pb::WebauthnCredentialSpecifics& passkey : passkeys) {
    // TODO(crbug.com/458337350): IsPasskeyValid is a temporary placeholder, add
    // a more universal function.
    if (!passkey_model_utils::IsPasskeyValid(passkey)) {
      result.errors.push_back(SpecificsToImportedPasskeyInfo(passkey));
      continue;
    }

    // TODO(crbug.com/458337350): Sanity check only matching credential ID.
    std::optional<sync_pb::WebauthnCredentialSpecifics> stored_passkey =
        passkey_model_->GetPasskeyByUserId(passkey.rp_id(), passkey.user_id());
    if (stored_passkey.has_value()) {
      result.conflicts.push_back(SpecificsToImportedPasskeyInfo(passkey));
      conflicting_passkeys_.push_back(
          {.stored_passkey = *std::move(stored_passkey),
           .incoming_passkey = std::move(passkey)});
      continue;
    }

    valid_passkeys_.push_back(std::move(passkey));
    result.valid_passkeys_amount++;
  }
  std::move(processing_callback).Run(result);
}

}  // namespace webauthn
