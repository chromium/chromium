// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/pickle.h"

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/compiler_specific.h"

namespace net {

namespace {

// See comment in pickle_unittest.cc for the justification for this type.
using FuzzType = std::set<std::tuple<bool,
                                     std::optional<int>,
                                     std::vector<std::string>,
                                     std::vector<uint16_t>>>;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // SAFETY: The fuzzer contract guarantees that `data` points to at least
  // `size` bytes.
  auto pickle =
      base::Pickle::WithUnownedBuffer(UNSAFE_BUFFERS(base::span(data, size)));
  auto result = ReadValueFromPickle<FuzzType>(pickle);
  return 0;
}

}  // namespace net
