// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORT_PROCESSING_RESULT_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORT_PROCESSING_RESULT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/webauthn/core/browser/import/imported_passkey_checker.h"

namespace webauthn {

// Attributes of a passkey to be displayed in the import UI.
struct ImportedPasskeyInfo {
  std::string rp_id;
  std::string user_name;
  ImportedPasskeyStatus status;
};

// Results of initial processing of to-be imported passkeys.
struct ImportProcessingResult {
  ImportProcessingResult();
  ImportProcessingResult(const ImportProcessingResult& other);
  ImportProcessingResult& operator=(const ImportProcessingResult& other);
  ImportProcessingResult(ImportProcessingResult&& other);
  ImportProcessingResult& operator=(ImportProcessingResult&& other);
  ~ImportProcessingResult();

  // Incoming passkeys that conflict with the passkeys stored in user's account.
  std::vector<ImportedPasskeyInfo> conflicts;

  // Incoming passkeys that cannot be imported.
  std::vector<ImportedPasskeyInfo> errors;

  // Amount of passkeys that have no errors or conflicts.
  int64_t valid_passkeys_amount = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_IMPORT_PROCESSING_RESULT_H_
