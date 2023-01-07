// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token.h"

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "url/gurl.h"

struct TestCase {
  TestCase() {
    CHECK(base::i18n::InitializeICU());
  }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  // Generates version 0,1,2,3 or 255.
  uint8_t version = (std::hash<std::string>()(str) % 5) - 1;
  blink::TrialToken::Parse(str, version);
  return 0;
}
