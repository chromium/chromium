// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/test_tag_registry.h"

#import <Foundation/Foundation.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#define MAYBE_testConditionalMethod testConditionalMethod

@interface SampleTestClassOne : NSObject
@end

@implementation SampleTestClassOne

TAG_SMOKE_TEST(SampleTestClassOne, testExistingMethod);
TAG_TEST(SampleTestClassOne, testExistingMethod, custom_tag);
- (void)testExistingMethod {
}

TAG_SMOKE_TEST(SampleTestClassOne, DISABLED_testDisabledMethod);
- (void)DISABLED_testDisabledMethod {
}

TAG_TEST(SampleTestClassOne, MAYBE_testConditionalMethod, custom_tag);
- (void)MAYBE_testConditionalMethod {
}

@end

@interface SampleTestClassTwo : NSObject
@end

@implementation SampleTestClassTwo

TAG_SMOKE_TEST(SampleTestClassTwo, testExistingMethod);
- (void)testExistingMethod {
}

@end

namespace testing {
namespace {

using TestTagRegistryTest = PlatformTest;

// Tests tagging and querying tags directly on a local registry instance.
TEST_F(TestTagRegistryTest, TagAndQueryDirectly) {
  TestTagRegistry registry;
  registry.TagTest("DirectTestCase", "testDirectExample", "smoke");

  EXPECT_TRUE(registry.HasTag("DirectTestCase", "testDirectExample", "smoke"));
  EXPECT_FALSE(registry.HasTag("DirectTestCase", "testDirectExample", "other"));
  EXPECT_FALSE(registry.HasTag("OtherTestCase", "testDirectExample", "smoke"));
  EXPECT_FALSE(registry.HasTag("DirectTestCase", "nonexistent", "smoke"));
}

// End-to-end test verifying static registration into the singleton via
// TAG_SMOKE_TEST and TAG_TEST macros.
TEST_F(TestTagRegistryTest, TagMacros) {
  const TestTagRegistry& registry = TestTagRegistry::GetInstance();

  // Both classes define a method named `testExistingMethod`, but they are
  // tracked separately.
  EXPECT_TRUE(
      registry.HasTag("SampleTestClassOne", "testExistingMethod", "smoke"));
  EXPECT_TRUE(
      registry.HasTag("SampleTestClassTwo", "testExistingMethod", "smoke"));

  // Explicitly tagged disabled test matches exact method name.
  EXPECT_TRUE(registry.HasTag("SampleTestClassOne",
                              "DISABLED_testDisabledMethod", "smoke"));
  EXPECT_FALSE(
      registry.HasTag("SampleTestClassOne", "testDisabledMethod", "smoke"));

  // Multiple tags on the same test.
  EXPECT_TRUE(registry.HasTag("SampleTestClassOne", "testExistingMethod",
                              "custom_tag"));
  EXPECT_FALSE(registry.HasTag("SampleTestClassTwo", "testExistingMethod",
                               "custom_tag"));

  // Macro arguments (like `MAYBE_` prefixes) are expanded before stringifying.
  EXPECT_TRUE(registry.HasTag("SampleTestClassOne", "testConditionalMethod",
                              "custom_tag"));
}

// Tests clearing a registry instance.
TEST_F(TestTagRegistryTest, ClearForTesting) {
  TestTagRegistry registry;
  registry.TagTest("SampleTestClassOne", "testExistingMethod", "smoke");
  EXPECT_TRUE(
      registry.HasTag("SampleTestClassOne", "testExistingMethod", "smoke"));

  registry.ClearForTesting();
  EXPECT_FALSE(
      registry.HasTag("SampleTestClassOne", "testExistingMethod", "smoke"));
}

}  // namespace
}  // namespace testing
