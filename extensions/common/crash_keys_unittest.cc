// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/crash_keys.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::crash_keys {

using crash_reporter::GetCrashKeyValue;

class CrashKeysTest : public testing::Test {
 public:
  void SetUp() override { crash_reporter::InitializeCrashKeys(); }

  void TearDown() override {
    // Breakpad doesn't properly support ResetCrashKeysForTesting() and usually
    // CHECK fails after it is called.
#if BUILDFLAG(USE_CRASHPAD_ANNOTATION)
    crash_reporter::ResetCrashKeysForTesting();
#endif
  }
};

TEST_F(CrashKeysTest, Extensions) {
  // Set three extensions.
  {
    std::set<std::string> extensions;
    extensions.insert("ext.1");
    extensions.insert("ext.2");
    extensions.insert("ext.3");

    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetCrashKeyValue("extension-1"));
    extensions.erase(GetCrashKeyValue("extension-2"));
    extensions.erase(GetCrashKeyValue("extension-3"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("3", GetCrashKeyValue("num-extensions"));
    EXPECT_TRUE(GetCrashKeyValue("extension-4").empty());
  }

  // Set more than the max switches.
  {
    std::set<std::string> extensions;
    const int kMax = 12;
    for (int i = 1; i <= kMax; ++i) {
      extensions.insert(base::StringPrintf("ext.%d", i));
    }
    crash_keys::SetActiveExtensions(extensions);

    for (int i = 1; i <= kMax; ++i) {
      extensions.erase(GetCrashKeyValue(base::StringPrintf("extension-%d", i)));
    }
    EXPECT_EQ(2u, extensions.size());

    EXPECT_EQ("12", GetCrashKeyValue("num-extensions"));
    EXPECT_TRUE(GetCrashKeyValue("extension-13").empty());
    EXPECT_TRUE(GetCrashKeyValue("extension-14").empty());
  }

  // Set fewer to ensure that old ones are erased.
  {
    std::set<std::string> extensions;
    for (int i = 1; i <= 5; ++i) {
      extensions.insert(base::StringPrintf("ext.%d", i));
    }
    crash_keys::SetActiveExtensions(extensions);

    extensions.erase(GetCrashKeyValue("extension-1"));
    extensions.erase(GetCrashKeyValue("extension-2"));
    extensions.erase(GetCrashKeyValue("extension-3"));
    extensions.erase(GetCrashKeyValue("extension-4"));
    extensions.erase(GetCrashKeyValue("extension-5"));
    EXPECT_EQ(0u, extensions.size());

    EXPECT_EQ("5", GetCrashKeyValue("num-extensions"));
    for (int i = 6; i < 20; ++i) {
      std::string key = base::StringPrintf("extension-%d", i);
      EXPECT_TRUE(GetCrashKeyValue(key).empty()) << key;
    }
  }
}

}  // namespace extensions::crash_keys
