// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/json_web_key.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// For disabling noisy logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  std::vector<uint8_t> license(data.begin(), data.end());
  std::vector<uint8_t> first_key;
  media::ExtractFirstKeyIdFromLicenseRequest(license, &first_key);

  std::string input(reinterpret_cast<const char*>(data.data()), data.size());
  media::KeyIdAndKeyPairs keys;
  media::CdmSessionType session_type;
  media::ExtractKeysFromJWKSet(input, &keys, &session_type);

  media::KeyIdList key_ids;
  std::string error_message;
  media::ExtractKeyIdsFromKeyIdsInitData(input, &key_ids, &error_message);

  return 0;
}
