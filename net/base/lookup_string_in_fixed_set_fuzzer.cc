// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/lookup_string_in_fixed_set.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/compiler_specific.h"
#include "net/base/registry_controlled_domains/effective_tld_names-inc.cc"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::LookupStringInFixedSet(net::registry_controlled_domains::kDafsa,
                              UNSAFE_BUFFERS(std::string_view(
                                  reinterpret_cast<const char*>(data), size)));
  return 0;
}
