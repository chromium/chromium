// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_network_connectivity_tracker.h"

#include "net/base/ip_address.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class QuicNetworkConnectivityTrackerTest : public TestWithTaskEnvironment {
 protected:
  test::ScopedMockNetworkChangeNotifier scoped_notifier_;
};

TEST_F(QuicNetworkConnectivityTrackerTest, InitialState) {
  QuicNetworkConnectivityTracker tracker;
  EXPECT_FALSE(tracker.HasQuicEverWorkedOnCurrentNetwork());
}

TEST_F(QuicNetworkConnectivityTrackerTest, OnQuicWorkingOnCurrentNetwork) {
  QuicNetworkConnectivityTracker tracker;

  tracker.OnQuicWorkingOnCurrentNetwork();
  EXPECT_TRUE(tracker.HasQuicEverWorkedOnCurrentNetwork());
}

TEST_F(QuicNetworkConnectivityTrackerTest, ResetQuicWorkingOnCurrentNetwork) {
  QuicNetworkConnectivityTracker tracker;
  tracker.OnQuicWorkingOnCurrentNetwork();

  EXPECT_TRUE(tracker.HasQuicEverWorkedOnCurrentNetwork());

  tracker.ResetQuicWorkingOnCurrentNetwork();
  EXPECT_FALSE(tracker.HasQuicEverWorkedOnCurrentNetwork());
}

TEST_F(QuicNetworkConnectivityTrackerTest, OnIPAddressChanged) {
  QuicNetworkConnectivityTracker tracker;
  tracker.OnQuicWorkingOnCurrentNetwork();
  EXPECT_TRUE(tracker.HasQuicEverWorkedOnCurrentNetwork());

  tracker.OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL);
  EXPECT_FALSE(tracker.HasQuicEverWorkedOnCurrentNetwork());
}

}  // namespace
}  // namespace net
