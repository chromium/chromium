// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/test_passkey_model.h"

#include <iterator>

#include "base/notreached.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"

namespace webauthn {

TestPasskeyModel::TestPasskeyModel() = default;
TestPasskeyModel::~TestPasskeyModel() = default;

void TestPasskeyModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TestPasskeyModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TestPasskeyModel::GetModelTypeControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
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

std::vector<sync_pb::WebauthnCredentialSpecifics>
TestPasskeyModel::GetPasskeysForRelyingPartyId(const std::string& rp_id) const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys;
  base::ranges::copy_if(
      credentials_, std::back_inserter(passkeys),
      [&rp_id](const auto& passkey) { return passkey.rp_id() == rp_id; });
  return passkey_model_utils::FilterShadowedCredentials(passkeys);
}

std::string TestPasskeyModel::AddNewPasskeyForTesting(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  credentials_.push_back(std::move(passkey));
  NotifyPasskeysChanged();
  return credentials_.back().credential_id();
}

bool TestPasskeyModel::DeletePasskey(const std::string& credential_id) {
  // Don't implement the shadow chain deletion logic. Instead, remove the
  // credential with the matching id.
  bool removed =
      std::erase_if(credentials_, [&credential_id](const auto& credential) {
        return credential.credential_id() == credential_id;
      }) > 0;
  NotifyPasskeysChanged();
  return removed;
}

bool TestPasskeyModel::UpdatePasskey(const std::string& credential_id,
                                     PasskeyChange change) {
  const auto credential_it = std::ranges::find_if(
      credentials_, [&credential_id](const auto& credential) {
        return credential.credential_id() == credential_id;
      });
  if (credential_it == credentials_.end()) {
    return false;
  }
  credential_it->set_user_name(std::move(change.user_name));
  credential_it->set_user_display_name(std::move(change.user_display_name));
  NotifyPasskeysChanged();
  return true;
}

void TestPasskeyModel::NotifyPasskeysChanged() {
  for (auto& observer : observers_) {
    observer.OnPasskeysChanged();
  }
}

}  // namespace webauthn
