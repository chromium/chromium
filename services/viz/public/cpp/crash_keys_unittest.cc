// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/crash_keys.h"

#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

TEST(CrashKeysTest, DeserializationCrashKeys) {
  crash_reporter::InitializeCrashKeysForTesting();
  viz::ClearDeserializationCrashKeys();

  // Up to 3 errors are stored in order.
  viz::SetDeserializationCrashKeyString("Error 1 (Innermost)");
  viz::SetDeserializationCrashKeyString("Error 2 (Middle)");
  viz::SetDeserializationCrashKeyString("Error 3 (Outermost)");
  viz::SetDeserializationCrashKeyString("Error 4 (Overflow)");

  EXPECT_EQ(crash_reporter::GetCrashKeyValue("viz_deserialization"),
            "Error 1 (Innermost)");
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("viz_deserialization_2"),
            "Error 2 (Middle)");
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("viz_deserialization_3"),
            "Error 3 (Outermost)");

  // Calling ClearDeserializationCrashKeys resets all keys and count.
  viz::ClearDeserializationCrashKeys();
  EXPECT_TRUE(crash_reporter::GetCrashKeyValue("viz_deserialization").empty());
  EXPECT_TRUE(
      crash_reporter::GetCrashKeyValue("viz_deserialization_2").empty());
  EXPECT_TRUE(
      crash_reporter::GetCrashKeyValue("viz_deserialization_3").empty());

  viz::SetDeserializationCrashKeyString("Next run error");
  EXPECT_EQ(crash_reporter::GetCrashKeyValue("viz_deserialization"),
            "Next run error");
  EXPECT_TRUE(
      crash_reporter::GetCrashKeyValue("viz_deserialization_2").empty());

  viz::ClearDeserializationCrashKeys();
}

}  // namespace
}  // namespace viz
