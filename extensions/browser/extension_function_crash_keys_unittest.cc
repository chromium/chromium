// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_crash_keys.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using crash_reporter::GetCrashKeyValue;

// The crash key stub implementation does not store keys so we cannot test the
// value of a key.
#if !BUILDFLAG(USE_CRASH_KEY_STUBS)
class ExtensionFunctionCrashKeysTest : public testing::Test {
 public:
  void SetUp() override { crash_reporter::InitializeCrashKeysForTesting(); }

  void TearDown() override { crash_reporter::ResetCrashKeysForTesting(); }
};

TEST_F(ExtensionFunctionCrashKeysTest, SingleCall) {
  extension_function_crash_keys::StartExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id1");

  extension_function_crash_keys::EndExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "");
}

TEST_F(ExtensionFunctionCrashKeysTest, MultipleCalls) {
  // Simulate an extension with two simultaneous in-flight calls.
  extension_function_crash_keys::StartExtensionFunctionCall("id1");
  extension_function_crash_keys::StartExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id1");

  // Clear the first call. There's still one in-flight.
  extension_function_crash_keys::EndExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id1");

  // Clear the second call. No requests are in-flight.
  extension_function_crash_keys::EndExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "");
}

TEST_F(ExtensionFunctionCrashKeysTest, MultipleExtensions) {
  // Create more in-flight calls than we have crash keys available. This is
  // common during startup when all extensions are loading.
  extension_function_crash_keys::StartExtensionFunctionCall("id1");
  extension_function_crash_keys::StartExtensionFunctionCall("id2");
  extension_function_crash_keys::StartExtensionFunctionCall("id3");
  extension_function_crash_keys::StartExtensionFunctionCall("id4");

  // Crash keys are set for the last 3 extensions. They are stored with the
  // most recent call first.
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id4");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-2"), "id3");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-3"), "id2");

  // Call `id1` again. Now it has 2 in-flight calls and is the most recent.
  extension_function_crash_keys::StartExtensionFunctionCall("id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id1");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-2"), "id4");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-3"), "id3");
  extension_function_crash_keys::EndExtensionFunctionCall("id1");

  // End all calls except `id4`.
  extension_function_crash_keys::EndExtensionFunctionCall("id1");
  extension_function_crash_keys::EndExtensionFunctionCall("id2");
  extension_function_crash_keys::EndExtensionFunctionCall("id3");

  // Only one crash key is set and the others are cleared.
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-1"), "id4");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-2"), "");
  EXPECT_EQ(GetCrashKeyValue("extension-function-caller-3"), "");
}
#endif  // !BUILDFLAG(USE_CRASH_KEY_STUBS)

}  // namespace extensions
