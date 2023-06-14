// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_TEST_PASSKEY_MODEL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_TEST_PASSKEY_MODEL_H_

#include <string>

#include "base/observer_list.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace webauthn {

class TestPasskeyModel : public PasskeyModel {
 public:
  TestPasskeyModel();
  ~TestPasskeyModel() override;

  // PasskeyModel:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetModelTypeControllerDelegate() override;
  base::flat_set<std::string> GetAllSyncIds() const override;
  std::vector<sync_pb::WebauthnCredentialSpecifics> GetAllPasskeys()
      const override;
  std::string AddNewPasskeyForTesting(
      sync_pb::WebauthnCredentialSpecifics passkey) override;
  bool DeletePasskey(const std::string& credential_id) override;
  bool UpdatePasskey(const std::string& credential_id,
                     PasskeyChange change) override;

 private:
  void NotifyPasskeysChanged();

  std::vector<sync_pb::WebauthnCredentialSpecifics> credentials_;
  base::ObserverList<Observer> observers_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER__TEST_PASSKEY_MODEL_H_
