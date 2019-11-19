// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "third_party/blink/renderer/core/page/create_window.h"

#include <gtest/gtest.h>
#include "third_party/blink/public/web/web_window_features.h"
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
    EXPECT_EQ(test.noopener,
              GetWindowFeaturesFromString(test.feature_string).noopener)
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
              GetWindowFeaturesFromString(test.feature_string).noreferrer)
        << "Testing '" << test.feature_string << "'";
  }
}

}  // namespace blink
