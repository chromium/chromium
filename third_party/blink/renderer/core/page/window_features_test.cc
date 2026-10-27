// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

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

TEST_F(WindowFeaturesTest, AlwaysOnTop) {
  ScopedWindowOpenAlwaysOnTopForTest always_on_top_enabled{true};

  static const struct {
    const char* feature_string;
    bool always_on_top;
  } kCases[] = {
      {"", false},
      {"something", false},
      {"notalwaysontop", false},
      {"alwaysontop", true},
      {"something, alwaysontop", true},
      {"alwaysontop, something", true},
      {"AlWaYsOnToP", true},
      {"alwaysontop=1", true},
      {"alwaysontop=true", true},
      {"alwaysontop=yes", true},
      {"alwaysontop=0", false},
      {"alwaysontop=false", false},
      {"alwaysontop=no", false},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.always_on_top,
              GetWindowFeaturesFromString(test.feature_string,
                                          /*dom_window=*/nullptr)
                  .always_on_top)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, Split) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{true};

  static const struct {
    const char* feature_string;
    bool is_split;
    bool is_popup;
  } kCases[] = {
      {"", false, false},
      {"something", false, true},
      {"notsplit", false, true},
      {"split", true, false},
      {"SpLiT", true, false},
      {"split=1", true, false},
      {"split=yes", true, false},
      {"split=0", false, true},
      {"split=no", false, true},
      {"split=1, split=0", false, true},
      {"split=0, split=1", true, false},
      // The legacy "any other feature means popup" heuristic doesn't apply to
      // split requests.
      {"split, something", true, false},
      {"something, split", true, false},
      {"split, width=400, height=300", true, false},
      {"split, toolbar=0, menubar=0", true, false},
      {"split, noopener", true, false},
      {"split, noreferrer", true, false},
      // An explicit popup request is the fallback.
      {"split, popup", true, true},
      {"popup, split", true, true},
      {"split, popup=1", true, true},
      {"split, popup=0", true, false},
      {"popup=0, split", true, false},
      {"popup", false, true},
  };

  for (const auto& test : kCases) {
    WebWindowFeatures features = GetWindowFeaturesFromString(
        test.feature_string, /*dom_window=*/nullptr);
    EXPECT_EQ(test.is_split, features.is_split)
        << "Testing '" << test.feature_string << "'";
    EXPECT_EQ(test.is_popup, features.is_popup)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, SplitPosition) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{true};

  static const struct {
    const char* feature_string;
    mojom::SplitTabPosition split_position;
  } kCases[] = {
      {"split", mojom::SplitTabPosition::kUnspecified},
      {"split, splitposition=left", mojom::SplitTabPosition::kLeft},
      {"split, splitposition=right", mojom::SplitTabPosition::kRight},
      {"split, splitposition=top", mojom::SplitTabPosition::kTop},
      {"split, splitposition=bottom", mojom::SplitTabPosition::kBottom},
      {"splitposition=left, split", mojom::SplitTabPosition::kLeft},
      {"split, SplitPosition=LeFt", mojom::SplitTabPosition::kLeft},
      {"split, splitposition", mojom::SplitTabPosition::kUnspecified},
      {"split, splitposition=", mojom::SplitTabPosition::kUnspecified},
      {"split, splitposition=center", mojom::SplitTabPosition::kUnspecified},
      {"split, splitposition=inline-end",
       mojom::SplitTabPosition::kUnspecified},
      {"split, splitposition=1", mojom::SplitTabPosition::kUnspecified},
      // The last occurrence wins, even if it is invalid.
      {"split, splitposition=left, splitposition=top",
       mojom::SplitTabPosition::kTop},
      {"split, splitposition=left, splitposition=foo",
       mojom::SplitTabPosition::kUnspecified},
      // The position is only reported along with a split request.
      {"splitposition=left", mojom::SplitTabPosition::kUnspecified},
      {"split=0, splitposition=left", mojom::SplitTabPosition::kUnspecified},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.split_position,
              GetWindowFeaturesFromString(test.feature_string,
                                          /*dom_window=*/nullptr)
                  .split_position)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, SplitSize) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{true};

  static const struct {
    const char* feature_string;
    std::optional<uint8_t> split_size;
  } kCases[] = {
      {"split", std::nullopt},
      {"split, splitsize=30", 30},
      {"split, splitsize=1", 1},
      {"split, splitsize=99", 99},
      {"split, splitsize=50", 50},
      {"splitsize=30, split", 30},
      {"split, SplitSize=30", 30},
      // Trailing garbage is ignored, as for other integer features.
      {"split, splitsize=30%", 30},
      {"split, splitsize=30.7", 30},
      // Invalid or out-of-range values are ignored.
      {"split, splitsize", std::nullopt},
      {"split, splitsize=", std::nullopt},
      {"split, splitsize=yes", std::nullopt},
      {"split, splitsize=abc", std::nullopt},
      {"split, splitsize=0", std::nullopt},
      {"split, splitsize=100", std::nullopt},
      {"split, splitsize=-30", std::nullopt},
      {"split, splitsize=1000", std::nullopt},
      // The last occurrence wins, even if it is invalid.
      {"split, splitsize=30, splitsize=40", 40},
      {"split, splitsize=30, splitsize=abc", std::nullopt},
      // The size is only reported along with a split request.
      {"splitsize=30", std::nullopt},
      {"split=0, splitsize=30", std::nullopt},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.split_size,
              GetWindowFeaturesFromString(test.feature_string,
                                          /*dom_window=*/nullptr)
                  .split_size)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, SplitDisabled) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{false};

  static const struct {
    const char* feature_string;
    bool is_popup;
  } kCases[] = {
      {"split", true},
      {"split, splitposition=left, splitsize=30", true},
      {"popup=0, split", false},
  };

  for (const auto& test : kCases) {
    WebWindowFeatures features = GetWindowFeaturesFromString(
        test.feature_string, /*dom_window=*/nullptr);
    EXPECT_FALSE(features.is_split)
        << "Testing '" << test.feature_string << "'";
    EXPECT_EQ(mojom::SplitTabPosition::kUnspecified, features.split_position)
        << "Testing '" << test.feature_string << "'";
    EXPECT_EQ(std::nullopt, features.split_size)
        << "Testing '" << test.feature_string << "'";
    EXPECT_EQ(test.is_popup, features.is_popup)
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, WindowFeaturesFromAttribute) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{true};

  // Empty string gives default features.
  {
    WebWindowFeatures features = GetWindowFeaturesFromAttribute("");
    EXPECT_FALSE(features.is_split);
    EXPECT_FALSE(features.is_popup);
    EXPECT_FALSE(features.noopener);
    EXPECT_FALSE(features.noreferrer);
    EXPECT_FALSE(features.explicit_opener);
  }

  // Split features are parsed.
  {
    WebWindowFeatures features = GetWindowFeaturesFromAttribute(
        "split, splitposition=left, splitsize=35");
    EXPECT_TRUE(features.is_split);
    EXPECT_FALSE(features.is_popup);
    EXPECT_EQ(mojom::SplitTabPosition::kLeft, features.split_position);
    EXPECT_EQ(35, features.split_size);
  }

  // Popup and bounds features are parsed.
  {
    WebWindowFeatures features = GetWindowFeaturesFromAttribute(
        "popup, left=10, top=20, width=300, height=400");
    EXPECT_FALSE(features.is_split);
    EXPECT_TRUE(features.is_popup);
    EXPECT_TRUE(features.x_set);
    EXPECT_EQ(10, features.x);
    EXPECT_TRUE(features.y_set);
    EXPECT_EQ(20, features.y);
    EXPECT_TRUE(features.width_set);
    EXPECT_EQ(300, features.width);
    EXPECT_TRUE(features.height_set);
    EXPECT_EQ(400, features.height);
  }

  // Bounds aliases (screenx, screeny, innerwidth, innerheight) are parsed,
  // and setting bounds alone does NOT apply the legacy popup heuristic.
  {
    WebWindowFeatures features = GetWindowFeaturesFromAttribute(
        "screenx=15, screeny=25, innerwidth=350, innerheight=450");
    EXPECT_FALSE(features.is_split);
    EXPECT_FALSE(features.is_popup);
    EXPECT_TRUE(features.x_set);
    EXPECT_EQ(15, features.x);
    EXPECT_TRUE(features.y_set);
    EXPECT_EQ(25, features.y);
    EXPECT_TRUE(features.width_set);
    EXPECT_EQ(350, features.width);
    EXPECT_TRUE(features.height_set);
    EXPECT_EQ(450, features.height);
  }

  // Disallowed features (noopener, noreferrer, opener, toolbar, menubar, etc.)
  // and unknown features are ignored and do NOT trigger the legacy popup
  // heuristic.
  static const char* const kDisallowedOrLegacyPopupCases[] = {
      "something",     "split=0",
      "popup=0",       "noopener",
      "noreferrer",    "opener",
      "toolbar=0",     "menubar=0",
      "location=0",    "resizable=0",
      "scrollbars=0",  "status=0",
      "alwaysontop=1", "background=1",
      "persistent=1",  "attributionsrc=https://example.com",
  };
  for (const char* feature_string : kDisallowedOrLegacyPopupCases) {
    WebWindowFeatures features = GetWindowFeaturesFromAttribute(feature_string);
    EXPECT_FALSE(features.is_split) << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.is_popup) << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.noopener) << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.noreferrer) << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.explicit_opener)
        << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.always_on_top)
        << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.background) << "Testing '" << feature_string << "'";
    EXPECT_FALSE(features.persistent) << "Testing '" << feature_string << "'";
  }
}

TEST_F(WindowFeaturesTest, WindowFeaturesFromAttributeDisabled) {
  ScopedWindowOpenSplitTabForTest split_tab_enabled{false};

  WebWindowFeatures features = GetWindowFeaturesFromAttribute(
      "split, splitposition=left, splitsize=30, popup, width=400, height=300");
  EXPECT_FALSE(features.is_split);
  EXPECT_FALSE(features.is_popup);
  EXPECT_EQ(mojom::SplitTabPosition::kUnspecified, features.split_position);
  EXPECT_EQ(std::nullopt, features.split_size);
  EXPECT_FALSE(features.width_set);
  EXPECT_FALSE(features.height_set);
}

}  // namespace blink
