// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/screen_resolution.h"

#include <stdint.h>

#include <limits>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(ScreenResolutionTest, Empty) {
  ScreenResolution resolution1(webrtc::DesktopSize(100, 100),
                               webrtc::DesktopVector(10, 10));
  EXPECT_FALSE(resolution1.IsEmpty());

  ScreenResolution resolution2(webrtc::DesktopSize(),
                               webrtc::DesktopVector(10, 10));
  EXPECT_TRUE(resolution2.IsEmpty());

  ScreenResolution resolution3(webrtc::DesktopSize(1, 1),
                               webrtc::DesktopVector(0, 0));
  EXPECT_TRUE(resolution3.IsEmpty());
}

TEST(ScreenResolutionTest, Scaling) {
  ScreenResolution resolution(webrtc::DesktopSize(100, 100),
                              webrtc::DesktopVector(10, 10));

  EXPECT_TRUE(webrtc::DesktopSize(50, 50).equals(
      resolution.ScaleDimensionsToDpi(webrtc::DesktopVector(5, 5))));

  EXPECT_TRUE(webrtc::DesktopSize(200, 200).equals(
      resolution.ScaleDimensionsToDpi(webrtc::DesktopVector(20, 20))));
}

TEST(ScreenResolutionTest, ScalingSaturation) {
  ScreenResolution resolution(webrtc::DesktopSize(10000000, 1000000),
                              webrtc::DesktopVector(1, 1));

  int32_t max_int = std::numeric_limits<int32_t>::max();
  EXPECT_TRUE(webrtc::DesktopSize(max_int, max_int)
                  .equals(resolution.ScaleDimensionsToDpi(
                      webrtc::DesktopVector(1000000, 1000000))));
}

}  // namespace remoting
