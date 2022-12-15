// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <fuzzer/FuzzedDataProvider.h>

#include "extensions/common/csp_validator.h"
#include "extensions/common/install_warning.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"

namespace extensions {

namespace {

// Performs common initialization that's shared between all runs.
struct Environment {
  IcuEnvironment icu_environment;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  const size_t kMaxSize = 10000;
  if (size > kMaxSize) {
    // Bail out if the input is too big (the exact limit is arbitrary), to avoid
    // going out of memory when the CSP validator produces many warnings.
    return 0;
  }
  FuzzedDataProvider fuzzed_data_provider(data, size);

  const std::string content_security_policy =
      fuzzed_data_provider.ConsumeRandomLengthString();
  const std::string manifest_key =
      fuzzed_data_provider.ConsumeRandomLengthString();

  std::vector<InstallWarning> install_warnings;
  csp_validator::SanitizeContentSecurityPolicy(
      content_security_policy, manifest_key,
      /*options=*/fuzzed_data_provider.ConsumeIntegralInRange(0, 4),
      &install_warnings);

  csp_validator::GetSandboxedPageCSPDisallowingRemoteSources(
      content_security_policy, manifest_key, &install_warnings);

  std::u16string error;
  csp_validator::DoesCSPDisallowRemoteCode(content_security_policy,
                                           manifest_key, &error);

  return 0;
}

}  // namespace extensions
