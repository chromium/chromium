// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <string_view>

#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

// Very simple fuzzer to allow us to (manually) test that string comparison
// instrumentation is working. If so, this should almost immediately crash,
// otherwise it will run almost forever because there's no real chance of
// hitting upon 'fish' by accident.
void StringCompare(std::string_view the_string) {
  if (the_string == "fish") {
    std::cout << "Found fish\n";
    exit(1);
  }
}

FUZZ_TEST(TestFuzzer, StringCompare);
