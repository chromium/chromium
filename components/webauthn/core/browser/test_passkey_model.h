// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_TEST_PASSKEY_MODEL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_TEST_PASSKEY_MODEL_H_

#include <string>

#include "base/observer_list.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

namespace webauthn {

class TestPasskeyModel : public PasskeyModel {
 public:
  TestPasskeyModel();
  ~TestPasskeyModel() override;

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
  void NotifyPasskeysChanged(const std::vector<PasskeyModelChange>& changes);
  void AddShadowedCredentialIdsToNewPasskey(
      sync_pb::WebauthnCredentialSpecifics& passkey);

  std::vector<sync_pb::WebauthnCredentialSpecifics> credentials_;
  base::ObserverList<Observer> observers_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_TEST_PASSKEY_MODEL_H_
