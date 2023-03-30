// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace syncer {
class ModelTypeControllerDelegate;
}

// PasskeyModel provides access to passkeys, which are represented as
// WebauthnCredentialSpecifics in Sync.
//
// By design, non-syncing passkeys are not supported (unlike passwords e.g.,
// which can be created on the local device only, without uploading them to
// Sync).
//
// Aside from Sync passkeys, there might be other WebAuthn platform credentials
// on the device. E.g., non-passkey credentials in the browser-provided
// authenticators on CrOS (u2fd) and macOS (//device/fido/mac); or platform
// credentials owned by Windows Hello or iCloud Keychain. None of these are
// accessible though PasskeyModel.
class PasskeyModel : public KeyedService {
 public:
  // Returns the sync ModelTypeControllerDelegate for the WEBAUTHN_CREDENTIAL
  // data type.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetModelTypeControllerDelegate() = 0;

  virtual base::flat_set<std::string> GetAllSyncIds() const = 0;
  virtual std::vector<sync_pb::WebauthnCredentialSpecifics> GetAllPasskeys()
      const = 0;
  virtual std::string AddNewPasskeyForTesting(
      sync_pb::WebauthnCredentialSpecifics passkey) = 0;
  virtual bool DeletePasskeyForTesting(std::string sync_id) = 0;
};

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_H_
