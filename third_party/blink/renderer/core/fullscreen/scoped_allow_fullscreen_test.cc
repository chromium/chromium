// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ScopedAllowFullscreenTest, InitialState) {
  EXPECT_FALSE(ScopedAllowFullscreen::FullscreenAllowedReason().has_value());
}

TEST(ScopedAllowFullscreenTest, ConstructOneScope) {
  ScopedAllowFullscreen scope(ScopedAllowFullscreen::kOrientationChange);

  EXPECT_EQ(ScopedAllowFullscreen::kOrientationChange,
            ScopedAllowFullscreen::FullscreenAllowedReason().value());
}

TEST(ScopedAllowFullscreenTest, MultipleScopesInTheSameScope) {
  ScopedAllowFullscreen scope1(ScopedAllowFullscreen::kOrientationChange);

  EXPECT_EQ(ScopedAllowFullscreen::kOrientationChange,
            ScopedAllowFullscreen::FullscreenAllowedReason().value());

  ScopedAllowFullscreen scope2(ScopedAllowFullscreen::kOrientationChange);

  EXPECT_EQ(ScopedAllowFullscreen::kOrientationChange,
            ScopedAllowFullscreen::FullscreenAllowedReason().value());
}

TEST(ScopedAllowFullscreenTest, DestructResetsState) {
  { ScopedAllowFullscreen scope(ScopedAllowFullscreen::kOrientationChange); }

  EXPECT_FALSE(ScopedAllowFullscreen::FullscreenAllowedReason().has_value());
}

TEST(ScopedAllowFullscreenTest, DestructResetsStateToPrevious) {
  ScopedAllowFullscreen scope(ScopedAllowFullscreen::kOrientationChange);
  { ScopedAllowFullscreen scope2(ScopedAllowFullscreen::kOrientationChange); }

  EXPECT_EQ(ScopedAllowFullscreen::kOrientationChange,
            ScopedAllowFullscreen::FullscreenAllowedReason().value());
}

}  // namespace blink
