// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORTER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORTER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace webauthn {

class PasskeyModel;
struct ImportProcessingResult;

// Handles the process of importing passkeys. This includes identifying
// conflicts with passkeys stored in user's account, verifying validity of
// incoming passkeys and required interaction with `PasskeyModel`.
//
// The caller should initiate the process by calling `StartImport`.
// TODO(crbug.com/458337350): Add more unit tests.
// TODO(crbug.com/458337350): Implement the API for resuming import after
// resolving conflicts.
class PasskeyImporter {
 public:
  using ProcessingCallback =
      base::OnceCallback<void(const ImportProcessingResult&)>;

  explicit PasskeyImporter(PasskeyModel& passkey_model);
  PasskeyImporter(const PasskeyImporter&) = delete;
  PasskeyImporter& operator=(const PasskeyImporter&) = delete;
  ~PasskeyImporter();

  // Processes `passkeys` async by splitting them into the following groups:
  // * passkeys that cannot be imported (e.g. missing private key)
  // * conflicting with already stored passkeys
  // * valid passkeys, ready to be imported
  // Caches the passkeys after processing and runs `processing_callback` to let
  // the caller display some UI, so the user can resolve conflicts (if any).
  void StartImport(std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
                   ProcessingCallback processing_callback);

 private:
  // Used to store a conflicting pair of passkeys, i.e. having matching `rp_id`
  // and `user_id`.
  struct ConflictingPasskeys {
    sync_pb::WebauthnCredentialSpecifics stored_passkey;
    sync_pb::WebauthnCredentialSpecifics incoming_passkey;
  };

  // Performs the async work described in `StartImport()`.
  void ProcessPasskeys(
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
      ProcessingCallback processing_callback);

  // Provides access to stored WebAuthn credentials.
  const raw_ref<PasskeyModel> passkey_model_;

  // Caches ready to be imported passkeys.
  std::vector<sync_pb::WebauthnCredentialSpecifics> valid_passkeys_;

  // Caches conflicting passkeys.
  std::vector<ConflictingPasskeys> conflicting_passkeys_;

  base::WeakPtrFactory<PasskeyImporter> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORTER_H_
