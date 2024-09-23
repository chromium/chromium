// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "net/base/url_util.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"
#include "url/url_canon.h"

struct Environment {
  IcuEnvironment icu_environment;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (size < 1) {
    return 0;
  }

  std::string_view host(reinterpret_cast<const char*>(data), size);
  url::CanonHostInfo host_info;
  net::CanonicalizeHost(host, &host_info);
  return 0;
}
