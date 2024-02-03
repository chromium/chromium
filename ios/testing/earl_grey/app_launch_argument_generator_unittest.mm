// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_argument_generator.h"

#import "base/feature_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
BASE_FEATURE(kTestOne, "test_one", base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

// Unittests related to generating app launch arguments.
class AppLaunchArgumentGeneratorTest : public PlatformTest {
 public:
  AppLaunchArgumentGeneratorTest() {}
  ~AppLaunchArgumentGeneratorTest() override {}
};

TEST_F(AppLaunchArgumentGeneratorTest, MergeMultipleEnabledFlags) {
  AppLaunchConfiguration configuration;
  configuration.additional_args.push_back("--enable-features=TestOne");
  configuration.additional_args.push_back("--enable-features=TestTwo");

  NSArray<NSString*>* arguments = ArgumentsFromConfiguration(configuration);

  bool found_enable_features = false;
  for (NSString* argument : arguments) {
    if ([argument hasPrefix:@"--enable-features"]) {
      if (found_enable_features) {
        ADD_FAILURE() << "Arguments has 2 --enable-features entries";
      }
      found_enable_features = true;
      EXPECT_NSEQ(argument, @"--enable-features=TestOne,TestTwo");
    }
  }
  EXPECT_TRUE(found_enable_features);
}

TEST_F(AppLaunchArgumentGeneratorTest, MergeEnabledAndFeatures) {
  AppLaunchConfiguration configuration;
  configuration.features_enabled.push_back(kTestOne);
  configuration.additional_args.push_back("--enable-features=TestTwo");

  NSArray<NSString*>* arguments = ArgumentsFromConfiguration(configuration);

  bool found_enable_features = false;
  for (NSString* argument : arguments) {
    if ([argument hasPrefix:@"--enable-features"]) {
      if (found_enable_features) {
        ADD_FAILURE() << "Arguments has 2 --enable-features entries";
      }
      found_enable_features = true;
      EXPECT_NSEQ(argument, @"--enable-features=test_one,TestTwo");
    }
  }
  EXPECT_TRUE(found_enable_features);
}
