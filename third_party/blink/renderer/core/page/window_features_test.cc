// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/page/create_window.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using WindowFeaturesTest = testing::Test;

TEST_F(WindowFeaturesTest, NoOpener) {
  static const struct {
    const char* feature_string;
    bool noopener;
  } kCases[] = {
      {"", false},
      {"something", false},
      {"something, something", false},
      {"notnoopener", false},
      {"noopener", true},
      {"something, noopener", true},
      {"noopener, something", true},
      {"NoOpEnEr", true},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.noopener, GetWindowFeaturesFromString(test.feature_string,
                                                         /*dom_window=*/nullptr)
                                 .noopener)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, NoReferrer) {
  static const struct {
    const char* feature_string;
    bool noopener;
    bool noreferrer;
  } kCases[] = {
      {"", false, false},
      {"something", false, false},
      {"something, something", false, false},
      {"notreferrer", false, false},
      {"noreferrer", true, true},
      {"something, noreferrer", true, true},
      {"noreferrer, something", true, true},
      {"NoReFeRrEr", true, true},
      {"noreferrer, noopener=0", true, true},
      {"noreferrer=0, noreferrer=1", true, true},
      {"noreferrer=1, noreferrer=0", false, false},
      {"noreferrer=1, noreferrer=0, noopener=1", true, false},
      {"something, noreferrer=1, noreferrer=0", false, false},
      {"noopener=1, noreferrer=1, noreferrer=0", true, false},
      {"noopener=0, noreferrer=1, noreferrer=0", false, false},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.noreferrer,
              GetWindowFeaturesFromString(test.feature_string,
                                          /*dom_window=*/nullptr)
                  .noreferrer)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, Opener) {
  ScopedRelOpenerBcgDependencyHintForTest explicit_opener_enabled{true};

  static const struct {
    const char* feature_string;
    bool explicit_opener;
  } kCases[] = {
      {"", false},
      {"something", false},
      {"notopener", false},
      {"noopener", false},
      {"opener", true},
      {"something, opener", true},
      {"opener, something", true},
      {"OpEnEr", true},
      {"noopener, opener", false},
      {"opener, noopener", false},
      {"noreferrer, opener", false},
      {"opener, noreferrer", false},
      {"noopener=0", false},
      {"noopener=0, opener", true},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.explicit_opener,
              GetWindowFeaturesFromString(test.feature_string,
                                          /*dom_window=*/nullptr)
                  .explicit_opener)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, PartitionedPopin) {
  for (const bool& partitioned_popins_enabled : {false, true}) {
    ScopedPartitionedPopinsForTest scoped_feature{partitioned_popins_enabled};
    WebWindowFeatures window_features =
        GetWindowFeaturesFromString("popin",
                                    /*dom_window=*/nullptr);
    EXPECT_EQ(partitioned_popins_enabled, window_features.is_partitioned_popin);
    EXPECT_EQ(true, window_features.is_popup);
    window_features = GetWindowFeaturesFromString("popin,popup=0",
                                                  /*dom_window=*/nullptr);
    EXPECT_EQ(partitioned_popins_enabled, window_features.is_partitioned_popin);
    EXPECT_EQ(partitioned_popins_enabled, window_features.is_popup);
  }
}

}  // namespace blink
