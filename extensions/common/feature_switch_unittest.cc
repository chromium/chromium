// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::FeatureSwitch;

namespace {

const char kSwitchName[] = "test-switch";

template <FeatureSwitch::DefaultValue T>
class FeatureSwitchTest : public testing::Test {
 public:
  FeatureSwitchTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        feature_(&command_line_, kSwitchName, T) {}

 protected:
  base::CommandLine command_line_;
  FeatureSwitch feature_;
};

typedef FeatureSwitchTest<FeatureSwitch::DEFAULT_DISABLED>
    FeatureSwitchDisabledTest;
typedef FeatureSwitchTest<FeatureSwitch::DEFAULT_ENABLED>
    FeatureSwitchEnabledTest;

}  // namespace

TEST_F(FeatureSwitchDisabledTest, NoSwitchValue) {
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, FalseSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "0");
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, GibberishSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "monkey");
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, Override) {
  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, TrueSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "1");
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchDisabledTest, TrimSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, " \t  1\n  ");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, NoSwitchValue) {
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, TrueSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "1");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, GibberishSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "monkey");
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, Override) {
  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_TRUE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, FalseSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "0");
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, true);
    EXPECT_TRUE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());

  {
    FeatureSwitch::ScopedOverride override(&feature_, false);
    EXPECT_FALSE(feature_.IsEnabled());
  }
  EXPECT_FALSE(feature_.IsEnabled());
}

TEST_F(FeatureSwitchEnabledTest, TrimSwitchValue) {
  command_line_.AppendSwitchASCII(kSwitchName, "\t\t 0 \n");
  EXPECT_FALSE(feature_.IsEnabled());
}
