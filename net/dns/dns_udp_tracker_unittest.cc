// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_udp_tracker.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class DnsUdpTrackerTest : public testing::Test {
 public:
  DnsUdpTrackerTest() {
    tracker_.set_tick_clock_for_testing(&test_tick_clock_);
  }

 protected:
  DnsUdpTracker tracker_;
  base::SimpleTestTickClock test_tick_clock_;
};

// Just testing that nothing crashes given some standard calls.
// TODO(ericorth@chromium.org): Actually test behavior once interesting
// side effects or data access is added.

TEST_F(DnsUdpTrackerTest, MatchingId) {
  static const uint16_t kId = 56;
  tracker_.RecordQuery(416 /* port */, kId);
  tracker_.RecordResponseId(kId /* query_id */, kId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedMismatch) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);
  static const uint16_t kNewId = 3456;
  tracker_.RecordQuery(3889 /* port */, kNewId);

  tracker_.RecordResponseId(kNewId /* query_id */, kOldId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedMismatch_Expired) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  test_tick_clock_.Advance(DnsUdpTracker::kMaxAge +
                           base::TimeDelta::FromMilliseconds(1));
  static const uint16_t kNewId = 3456;
  tracker_.RecordQuery(3889 /* port */, kNewId);

  tracker_.RecordResponseId(kNewId /* query_id */, kOldId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedMismatch_Full) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  uint16_t port = 124;
  uint16_t id = 3457;
  for (size_t i = 0; i < DnsUdpTracker::kMaxRecordedQueries; ++i) {
    tracker_.RecordQuery(++port, ++id);
  }

  tracker_.RecordResponseId(id /* query_id */, kOldId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, UnknownMismatch) {
  static const uint16_t kId = 4332;
  tracker_.RecordQuery(10014 /* port */, kId);
  tracker_.RecordResponseId(kId /* query_id */, 743 /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedPort) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  static const uint16_t kId = 580;
  tracker_.RecordQuery(kPort, kId);
  tracker_.RecordResponseId(kId /* query_id */, kId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedPort_Expired) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  test_tick_clock_.Advance(DnsUdpTracker::kMaxAge +
                           base::TimeDelta::FromMilliseconds(1));
  static const uint16_t kId = 580;
  tracker_.RecordQuery(kPort, kId);
  tracker_.RecordResponseId(kId /* query_id */, kId /* response_id */);
}

TEST_F(DnsUdpTrackerTest, ReusedPort_Full) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  uint16_t port = 124;
  uint16_t id = 3457;
  for (size_t i = 0; i < DnsUdpTracker::kMaxRecordedQueries; ++i) {
    tracker_.RecordQuery(++port, ++id);
  }

  tracker_.RecordQuery(kPort, ++id);
  tracker_.RecordResponseId(id /* query_id */, id /* response_id */);
}

}  // namespace

}  // namespace net
