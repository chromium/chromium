// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/import/passkey_importer.h"

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webauthn/core/browser/import/import_processing_result.h"
#include "components/webauthn/core/browser/import/imported_passkey_checker.h"
#include "components/webauthn/core/browser/import/passkey_import_candidate.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "crypto/keypair.h"

namespace webauthn {
namespace {

ImportedPasskeyInfo CandidateToImportedPasskeyInfo(
    const PasskeyImportCandidate& candidate,
    ImportedPasskeyStatus status) {
  return {.rp_id = candidate.rp_id,
          .user_name = candidate.user_name,
          .status = status};
}

sync_pb::WebauthnCredentialSpecifics CandidateToSpecifics(
    const PasskeyImportCandidate& candidate) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(
      base::RandBytesAsString(webauthn::passkey_model_utils::kSyncIdLength));
  passkey.set_credential_id(std::string(candidate.credential_id.begin(),
                                        candidate.credential_id.end()));
  passkey.set_user_id(
      std::string(candidate.user_id.begin(), candidate.user_id.end()));
  passkey.set_rp_id(candidate.rp_id);
  passkey.set_user_name(candidate.user_name);
  passkey.set_user_display_name(candidate.user_display_name);
  passkey.set_creation_time(candidate.creation_time);
  return passkey;
}

void RecordPasskeyImportError(const PasskeyImportCandidate& candidate,
                              ImportedPasskeyStatus status,
                              ImportProcessingResult& result) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.CredentialExchange.PasskeyImportStatus", status);
  result.errors.push_back(CandidateToImportedPasskeyInfo(candidate, status));
}

}  // namespace

PasskeyImporter::PasskeyImporter(PasskeyModel& passkey_model)
    : passkey_model_(passkey_model) {}

PasskeyImporter::~PasskeyImporter() = default;

void PasskeyImporter::StartImport(std::vector<PasskeyImportCandidate> passkeys,
                                  std::vector<uint8_t> trusted_vault_key,
                                  ProcessingCallback processing_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasskeyImporter::ProcessPasskeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(passkeys),
                     std::move(trusted_vault_key),
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
    std::vector<PasskeyImportCandidate> passkeys,
    std::vector<uint8_t> trusted_vault_key,
    ProcessingCallback processing_callback) {
  ImportProcessingResult result;
  for (const PasskeyImportCandidate& candidate : passkeys) {
    ImportedPasskeyStatus status = CheckImportedPasskey(candidate);
    if (status != ImportedPasskeyStatus::kOk) {
      RecordPasskeyImportError(candidate, status, result);
      continue;
    }

    sync_pb::WebauthnCredentialSpecifics passkey =
        CandidateToSpecifics(candidate);
    sync_pb::WebauthnCredentialSpecifics_Encrypted encrypted;
    encrypted.set_private_key(candidate.private_key.data(),
                              candidate.private_key.size());
    if (!webauthn::passkey_model_utils::EncryptWebauthnCredentialSpecificsData(
            trusted_vault_key, encrypted, &passkey)) {
      RecordPasskeyImportError(
          candidate, ImportedPasskeyStatus::kEncryptionFailed, result);
      continue;
    }

    if (passkey_model_
            ->GetPasskey(PasskeyModel::AnyRp(), passkey.credential_id(),
                         PasskeyModel::ShadowedCredentials::kInclude)
            .has_value()) {
      duplicate_passkey_count_++;
      continue;
    }

    std::vector<sync_pb::WebauthnCredentialSpecifics> existing_passkeys =
        passkey_model_->GetPasskeys(
            passkey.rp_id(), PasskeyModel::ShadowedCredentials::kExclude);
    if (std::ranges::any_of(
            existing_passkeys, [&](const auto& existing_passkey) {
              return existing_passkey.user_id() == passkey.user_id();
            })) {
      result.conflicts.push_back(
          CandidateToImportedPasskeyInfo(candidate, status));
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

  size_t imported_passkeys_count = valid_passkeys_.size() +
                                   selected_conflicting_passkey_ids.size() +
                                   duplicate_passkey_count_;
  std::move(passkeys_imported_callback)
      .Run(static_cast<int>(imported_passkeys_count));

  base::UmaHistogramCounts1000(
      "WebAuthentication.CredentialExchange.PasskeyConflictsCount",
      static_cast<int>(conflicting_passkeys_.size()));
  base::UmaHistogramCounts1000(
      "WebAuthentication.CredentialExchange.PasskeyConflictsResolvedCount",
      static_cast<int>(selected_conflicting_passkey_ids.size()));
  base::UmaHistogramCounts1000(
      "WebAuthentication.CredentialExchange.PasskeyDuplicatesCount",
      static_cast<int>(duplicate_passkey_count_));
  base::UmaHistogramCounts1000(
      "WebAuthentication.CredentialExchange.PasskeysImportedCount",
      static_cast<int>(imported_passkeys_count));
}

}  // namespace webauthn
