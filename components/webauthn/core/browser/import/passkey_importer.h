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
// TODO(crbug.com/458337350): Add metrics.
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

  // Finalizes the import process by actually storing the previously cached
  // `valid_passkeys_` and the subset of `conflicting_passkeys_` with the ids
  // provided on `selected_conflicting_passkey_ids`. Runs
  // `passkeys_imported_callback` informing how many passkeys were actually
  // imported.
  void FinishImport(std::vector<int> selected_conflicting_passkey_ids,
                    base::OnceCallback<void(int)> passkeys_imported_callback);

 private:
  // Performs the async work described in `StartImport()`.
  void ProcessPasskeys(
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
      ProcessingCallback processing_callback);

  // Performs the async work described in `FinishImport()`.
  void ImportPasskeys(std::vector<int> selected_conflicting_passkey_ids,
                      base::OnceCallback<void(int)> passkeys_imported_callback);

  // Provides access to stored WebAuthn credentials.
  const raw_ref<PasskeyModel> passkey_model_;

  // Caches ready to be imported passkeys.
  std::vector<sync_pb::WebauthnCredentialSpecifics> valid_passkeys_;

  // Caches passkeys present on the import list that are conflicting with the
  // already stored passkeys (e.g. having matching `user_id` and `rp_id`).
  std::vector<sync_pb::WebauthnCredentialSpecifics> conflicting_passkeys_;

  base::WeakPtrFactory<PasskeyImporter> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORTER_H_
