// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_udp_tracker.h"

#include "base/test/simple_test_tick_clock.h"
#include "net/base/net_errors.h"
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

TEST_F(DnsUdpTrackerTest, MatchingId) {
  uint16_t port = 416;
  uint16_t id = 56;
  for (size_t i = 0; i < DnsUdpTracker::kRecognizedIdMismatchThreshold; ++i) {
    tracker_.RecordQuery(++port, ++id);
    tracker_.RecordResponseId(id /* query_id */, id /* response_id */);
    EXPECT_FALSE(tracker_.low_entropy());
  }
}

TEST_F(DnsUdpTrackerTest, ReusedMismatches) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  uint16_t port = 3889;
  uint16_t id = 3456;
  for (size_t i = 0; i < DnsUdpTracker::kRecognizedIdMismatchThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordQuery(++port, ++id);
    tracker_.RecordResponseId(id /* query_id */, kOldId /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, ReusedMismatches_Expired) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  test_tick_clock_.Advance(DnsUdpTracker::kMaxAge + base::Milliseconds(1));

  uint16_t port = 3889;
  uint16_t id = 3456;

  // Because the query record has expired, the ID should be treated as
  // unrecognized.
  for (size_t i = 0; i < DnsUdpTracker::kUnrecognizedIdMismatchThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordQuery(++port, ++id);
    tracker_.RecordResponseId(id /* query_id */, kOldId /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

// Test for ID mismatches using an ID still kept in recorded queries, but not
// recent enough to be considered reognized.
TEST_F(DnsUdpTrackerTest, ReusedMismatches_Old) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  test_tick_clock_.Advance(DnsUdpTracker::kMaxRecognizedIdAge +
                           base::Milliseconds(1));

  uint16_t port = 3889;
  uint16_t id = 3456;

  // Expect the ID to be treated as unrecognized.
  for (size_t i = 0; i < DnsUdpTracker::kUnrecognizedIdMismatchThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordQuery(++port, ++id);
    tracker_.RecordResponseId(id /* query_id */, kOldId /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, ReusedMismatches_Full) {
  static const uint16_t kOldId = 786;
  tracker_.RecordQuery(123 /* port */, kOldId);

  uint16_t port = 124;
  uint16_t id = 3457;
  for (size_t i = 0; i < DnsUdpTracker::kMaxRecordedQueries; ++i) {
    tracker_.RecordQuery(++port, ++id);
  }

  // Expect the ID to be treated as unrecognized.
  for (size_t i = 0; i < DnsUdpTracker::kUnrecognizedIdMismatchThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordResponseId(id /* query_id */, kOldId /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, UnknownMismatches) {
  uint16_t port = 10014;
  uint16_t id = 4332;
  for (size_t i = 0; i < DnsUdpTracker::kUnrecognizedIdMismatchThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordQuery(++port, ++id);
    tracker_.RecordResponseId(id /* query_id */, 743 /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, ReusedPort) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  uint16_t id = 580;
  for (int i = 0; i < DnsUdpTracker::kPortReuseThreshold; ++i) {
    EXPECT_FALSE(tracker_.low_entropy());
    tracker_.RecordQuery(kPort, ++id);
    tracker_.RecordResponseId(id /* query_id */, id /* response_id */);
  }

  EXPECT_TRUE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, ReusedPort_Expired) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  test_tick_clock_.Advance(DnsUdpTracker::kMaxAge + base::Milliseconds(1));

  EXPECT_FALSE(tracker_.low_entropy());

  uint16_t id = 580;
  for (int i = 0; i < DnsUdpTracker::kPortReuseThreshold; ++i) {
    tracker_.RecordQuery(kPort, ++id);
    tracker_.RecordResponseId(id /* query_id */, id /* response_id */);
    EXPECT_FALSE(tracker_.low_entropy());
  }
}

TEST_F(DnsUdpTrackerTest, ReusedPort_Full) {
  static const uint16_t kPort = 2135;
  tracker_.RecordQuery(kPort, 579 /* query_id */);

  uint16_t port = 124;
  uint16_t id = 3457;
  for (size_t i = 0; i < DnsUdpTracker::kMaxRecordedQueries; ++i) {
    tracker_.RecordQuery(++port, ++id);
  }

  EXPECT_FALSE(tracker_.low_entropy());

  for (int i = 0; i < DnsUdpTracker::kPortReuseThreshold; ++i) {
    tracker_.RecordQuery(kPort, ++id);
    tracker_.RecordResponseId(id /* query_id */, id /* response_id */);
    EXPECT_FALSE(tracker_.low_entropy());
  }
}

TEST_F(DnsUdpTrackerTest, ConnectionError) {
  tracker_.RecordConnectionError(ERR_FAILED);

  EXPECT_FALSE(tracker_.low_entropy());
}

TEST_F(DnsUdpTrackerTest, ConnectionError_InsufficientResources) {
  tracker_.RecordConnectionError(ERR_INSUFFICIENT_RESOURCES);

  EXPECT_TRUE(tracker_.low_entropy());
}

}  // namespace

}  // namespace net
