// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace android {

TEST(NetworkLibraryTest, CaptivePortal) {
  EXPECT_FALSE(android::GetIsCaptivePortal());
}

TEST(NetworkLibraryTest, GetWifiSignalLevel) {
  base::Optional<int32_t> signal_strength = android::GetWifiSignalLevel();
  if (!signal_strength.has_value())
    return;
  EXPECT_LE(0, signal_strength.value());
  EXPECT_GE(4, signal_strength.value());
}

}  // namespace android

}  // namespace net
