// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORT_CANDIDATE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORT_CANDIDATE_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace webauthn {

// Represents a candidate passkey that is about to be imported.
struct PasskeyImportCandidate {
  std::string rp_id;
  std::string user_name;
  std::string user_display_name;
  std::vector<uint8_t> credential_id;
  std::vector<uint8_t> user_id;
  std::vector<uint8_t> private_key;
  int64_t creation_time = 0;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMPORT_PASSKEY_IMPORT_CANDIDATE_H_
