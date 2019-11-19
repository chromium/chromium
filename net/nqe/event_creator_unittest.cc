// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/event_creator.h"

#include "base/time/time.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace nqe {

namespace internal {

namespace {

// Returns the number of entries in |net_log| that have type set to
// |NetLogEventType::NETWORK_QUALITY_CHANGED|.
int GetNetworkQualityChangedEntriesCount(BoundTestNetLog* net_log) {
  return net_log->GetEntriesWithType(NetLogEventType::NETWORK_QUALITY_CHANGED)
      .size();
}

// Verify that the net log events are recorded correctly.
TEST(NetworkQualityEstimatorEventCreatorTest, Notified) {
  // std::unique_ptr<BoundTestNetLog>
  // net_log(std::make_unique<BoundTestNetLog>());
  BoundTestNetLog net_log;

  EventCreator event_creator(net_log.bound());

  NetworkQuality network_quality_100(base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(100),
                                     100);

  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_2G, network_quality_100);
  EXPECT_EQ(1, GetNetworkQualityChangedEntriesCount(&net_log));

  // No new entry should be created since the network quality has not changed.
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_2G, network_quality_100);
  EXPECT_EQ(1, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should be created since effective connection type has changed.
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_100);
  EXPECT_EQ(2, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should not be created since HTTP RTT has not changed
  // meaningfully.
  NetworkQuality network_quality_http_rtt_110(
      base::TimeDelta::FromMilliseconds(110),
      base::TimeDelta::FromMilliseconds(100), 100);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_http_rtt_110);
  EXPECT_EQ(2, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should be created since HTTP RTT has changed meaningfully.
  NetworkQuality network_quality_http_rtt_300(
      base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(100), 100);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_http_rtt_300);
  EXPECT_EQ(3, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should be created since transport RTT has changed meaningfully.
  NetworkQuality network_quality_transport_rtt_300(
      base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300), 100);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_transport_rtt_300);
  EXPECT_EQ(4, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should be created since bandwidth has changed meaningfully.
  NetworkQuality network_quality_kbps_300(
      base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300), 300);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_kbps_300);
  EXPECT_EQ(5, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should not be created since network quality has not changed
  // meaningfully.
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_kbps_300);
  EXPECT_EQ(5, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should be created since bandwidth has changed meaningfully.
  NetworkQuality network_quality_kbps_2000(
      base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300), 2000);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_kbps_2000);
  EXPECT_EQ(6, GetNetworkQualityChangedEntriesCount(&net_log));

  // A new entry should not be created since bandwidth has not changed by more
  // than 20%.
  NetworkQuality network_quality_kbps_2200(
      base::TimeDelta::FromMilliseconds(300),
      base::TimeDelta::FromMilliseconds(300), 2200);
  event_creator.MaybeAddNetworkQualityChangedEventToNetLog(
      EFFECTIVE_CONNECTION_TYPE_3G, network_quality_kbps_2200);
  EXPECT_EQ(6, GetNetworkQualityChangedEntriesCount(&net_log));
}

}  // namespace

}  // namespace internal

}  // namespace nqe

}  // namespace net
