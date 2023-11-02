// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_QUALITY_TRACKER_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_QUALITY_TRACKER_H_

#include "services/network/public/cpp/network_quality_tracker.h"

namespace network {

// Test version of NetworkQualityTracker without the network service instance.
class TestNetworkQualityTracker : public NetworkQualityTracker {
 public:
  TestNetworkQualityTracker();
  ~TestNetworkQualityTracker() override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_QUALITY_TRACKER_H_
