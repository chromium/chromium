// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/i18n/icu_util.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

namespace {

struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }

  // Initialize the "at exit manager" singleton used by the tested code.
  base::AtExitManager at_exit_manager;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider fuzzed_data_provider(data, size);

  URLPattern url_pattern(
      /*valid_schemes=*/fuzzed_data_provider.ConsumeIntegral<int>());
  if (url_pattern.Parse(fuzzed_data_provider.ConsumeRandomLengthString()) !=
      URLPattern::ParseResult::kSuccess) {
    return 0;
  }

  GURL url(fuzzed_data_provider.ConsumeRandomLengthString());
  url_pattern.MatchesURL(url);

  return 0;
}

}  // namespace extensions
