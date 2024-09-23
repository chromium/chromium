// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Call GetDomainAndRegistry() twice - once with each filter type to ensure
  // both code paths are exercised.
  net::registry_controlled_domains::GetDomainAndRegistry(
      std::string_view(reinterpret_cast<const char*>(data), size),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  net::registry_controlled_domains::GetDomainAndRegistry(
      std::string_view(reinterpret_cast<const char*>(data), size),
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  return 0;
}
