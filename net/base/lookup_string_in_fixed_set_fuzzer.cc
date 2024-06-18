// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/base/lookup_string_in_fixed_set.h"

namespace {
#include "net/base/registry_controlled_domains/effective_tld_names-inc.cc"
}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::LookupStringInFixedSet(kDafsa, reinterpret_cast<const char*>(data),
                              size);
  return 0;
}
