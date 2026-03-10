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

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace net {

namespace {

// See comment in pickle_unittest.cc for the justification for this type.
using FuzzType = std::set<std::tuple<bool,
                                     std::optional<int>,
                                     std::vector<std::string>,
                                     std::vector<uint16_t>>>;

}  // namespace

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  auto iter = base::PickleIterator::WithData(data);
  auto result = ReadValueFromPickle<FuzzType>(iter);
  return 0;
}

}  // namespace net
