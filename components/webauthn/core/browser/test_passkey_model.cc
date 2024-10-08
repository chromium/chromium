// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/test_passkey_model.h"

#include <iterator>
#include <optional>

#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace webauthn {

TestPasskeyModel::TestPasskeyModel() = default;

TestPasskeyModel::~TestPasskeyModel() {
  for (auto& observer : observers_) {
    observer.OnPasskeyModelShuttingDown();
  }
}

void TestPasskeyModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TestPasskeyModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TestPasskeyModel::GetDataTypeControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool TestPasskeyModel::IsReady() const {
  return true;
}

bool TestPasskeyModel::IsEmpty() const {
  return credentials_.empty();
}

base::flat_set<std::string> TestPasskeyModel::GetAllSyncIds() const {
  base::flat_set<std::string> ids;
  for (const auto& credential : credentials_) {
    ids.emplace(credential.sync_id());
  }
  return ids;
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
TestPasskeyModel::GetAllPasskeys() const {
  return credentials_;
}

std::optional<sync_pb::WebauthnCredentialSpecifics>
TestPasskeyModel::GetPasskeyByCredentialId(
    const std::string& rp_id,
    const std::string& credential_id) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> rp_passkeys;
  base::ranges::copy_if(
      credentials_, std::back_inserter(rp_passkeys),
      [&rp_id](const auto& passkey) { return passkey.rp_id() == rp_id; });
  rp_passkeys = passkey_model_utils::FilterShadowedCredentials(rp_passkeys);
  std::vector<sync_pb::WebauthnCredentialSpecifics> result;
  base::ranges::copy_if(rp_passkeys, std::back_inserter(result),
                        [&credential_id](const auto& passkey) {
                          return passkey.credential_id() == credential_id;
                        });
  if (result.empty()) {
    return std::nullopt;
  }
  CHECK_EQ(result.size(), 1u);
  return result.front();
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
TestPasskeyModel::GetPasskeysForRelyingPartyId(const std::string& rp_id) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  base::ranges::copy_if(
      credentials_, std::back_inserter(passkeys),
      [&rp_id](const auto& passkey) { return passkey.rp_id() == rp_id; });
  return passkey_model_utils::FilterShadowedCredentials(passkeys);
}

sync_pb::WebauthnCredentialSpecifics TestPasskeyModel::CreatePasskey(
    std::string_view rp_id,
    const UserEntity& user_entity,
    base::span<const uint8_t> trusted_vault_key,
    int32_t trusted_vault_key_version,
    std::vector<uint8_t>* public_key_spki_der_out) {
  auto [specifics, public_key_spki_der] =
      webauthn::passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          rp_id, user_entity, trusted_vault_key, trusted_vault_key_version);

  AddShadowedCredentialIdsToNewPasskey(specifics);
  credentials_.push_back(specifics);

  NotifyPasskeysChanged(
      {PasskeyModelChange(PasskeyModelChange::ChangeType::ADD, specifics)});

  if (public_key_spki_der_out != nullptr) {
    *public_key_spki_der_out = std::move(public_key_spki_der);
  }
  return specifics;
}

void TestPasskeyModel::CreatePasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  AddShadowedCredentialIdsToNewPasskey(passkey);
  credentials_.push_back(passkey);
  NotifyPasskeysChanged(
      {PasskeyModelChange(PasskeyModelChange::ChangeType::ADD, passkey)});
}

std::string TestPasskeyModel::AddNewPasskeyForTesting(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  credentials_.push_back(passkey);
  NotifyPasskeysChanged({PasskeyModelChange(PasskeyModelChange::ChangeType::ADD,
                                            std::move(passkey))});
  return credentials_.back().credential_id();
}

bool TestPasskeyModel::DeletePasskey(const std::string& credential_id,
                                     const base::Location& location) {
  // Don't implement the shadow chain deletion logic. Instead, remove the
  // credential with the matching id.
  const auto credential_it =
      base::ranges::find(credentials_, credential_id,
                         &sync_pb::WebauthnCredentialSpecifics::credential_id);
  if (credential_it == credentials_.end()) {
    return false;
  }
  PasskeyModelChange change(PasskeyModelChange::ChangeType::REMOVE,
                            *credential_it);
  credentials_.erase(credential_it);
  NotifyPasskeysChanged({std::move(change)});
  return true;
}

void TestPasskeyModel::DeleteAllPasskeys() {
  credentials_.clear();
}

bool TestPasskeyModel::UpdatePasskey(const std::string& credential_id,
                                     PasskeyUpdate change,
                                     bool updated_by_user) {
  const auto credential_it =
      base::ranges::find(credentials_, credential_id,
                         &sync_pb::WebauthnCredentialSpecifics::credential_id);
  if (credential_it == credentials_.end()) {
    return false;
  }
  if (credential_it->edited_by_user() && !updated_by_user) {
    return false;
  }
  credential_it->set_user_name(std::move(change.user_name));
  credential_it->set_user_display_name(std::move(change.user_display_name));
  credential_it->set_edited_by_user(updated_by_user);
  NotifyPasskeysChanged({PasskeyModelChange(
      PasskeyModelChange::ChangeType::UPDATE, *credential_it)});
  return true;
}

bool TestPasskeyModel::UpdatePasskeyTimestamp(const std::string& credential_id,
                                              base::Time last_used_time) {
  const auto credential_it =
      base::ranges::find(credentials_, credential_id,
                         &sync_pb::WebauthnCredentialSpecifics::credential_id);
  if (credential_it == credentials_.end()) {
    return false;
  }

  credential_it->set_last_used_time_windows_epoch_micros(
      last_used_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  NotifyPasskeysChanged({PasskeyModelChange(
      PasskeyModelChange::ChangeType::UPDATE, *credential_it)});
  return true;
}

void TestPasskeyModel::NotifyPasskeysChanged(
    const std::vector<PasskeyModelChange>& changes) {
  for (auto& observer : observers_) {
    observer.OnPasskeysChanged(changes);
  }
}

void TestPasskeyModel::AddShadowedCredentialIdsToNewPasskey(
    sync_pb::WebauthnCredentialSpecifics& passkey) {
  for (const auto& existing_passkey : credentials_) {
    if (existing_passkey.rp_id() == passkey.rp_id() &&
        existing_passkey.user_id() == passkey.user_id()) {
      passkey.add_newly_shadowed_credential_ids(
          existing_passkey.credential_id());
    }
  }
}

}  // namespace webauthn
