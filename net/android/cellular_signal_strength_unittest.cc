// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/cellular_signal_strength.h"

#include <stdint.h>

#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

TEST(CellularSignalStrengthAndroidTest, SignalStrengthLevelTest) {
  absl::optional<int32_t> signal_strength =
      android::cellular_signal_strength::GetSignalStrengthLevel();

  // Signal strength is unavailable if the device does not have an active
  // cellular connection.
  if (!NetworkChangeNotifier::IsConnectionCellular(
          NetworkChangeNotifier::GetConnectionType())) {
    EXPECT_FALSE(signal_strength);
    return;
  }

  EXPECT_TRUE(signal_strength);
  EXPECT_LE(0, signal_strength.value());
  EXPECT_GE(4, signal_strength.value());
}

}  // namespace

}  // namespace net
