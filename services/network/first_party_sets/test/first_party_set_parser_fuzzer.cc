// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_set_parser.h"

#include <cstdint>
#include <memory>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

namespace network {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece string_input(reinterpret_cast<const char*>(data), size);
  FirstPartySetParser::ParsePreloadedSets(string_input);
  return 0;
}

}  // namespace network
