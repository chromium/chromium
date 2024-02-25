// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

static void StringsAlwaysOccupyPositiveSpace(std::string input) {
  EXPECT_TRUE(input.size() >= 0);
}

FUZZ_TEST(FuzzTestSmokeTest, StringsAlwaysOccupyPositiveSpace);
