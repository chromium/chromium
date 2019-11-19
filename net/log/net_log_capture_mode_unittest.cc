// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_capture_mode.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(NetLogCaptureMode, Default) {
  NetLogCaptureMode mode = NetLogCaptureMode::kDefault;

  EXPECT_FALSE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_FALSE(NetLogCaptureIncludesSocketBytes(mode));
}

TEST(NetLogCaptureMode, IncludeSensitive) {
  NetLogCaptureMode mode = NetLogCaptureMode::kIncludeSensitive;

  EXPECT_TRUE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_FALSE(NetLogCaptureIncludesSocketBytes(mode));
}

TEST(NetLogCaptureMode, Everything) {
  NetLogCaptureMode mode = NetLogCaptureMode::kEverything;

  EXPECT_TRUE(NetLogCaptureIncludesSensitive(mode));
  EXPECT_TRUE(NetLogCaptureIncludesSocketBytes(mode));
}

}  // namespace

}  // namespace net
