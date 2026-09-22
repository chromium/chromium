// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/web_transport_send_stats_tracker.h"

#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

constexpr quic::QuicStreamId kStreamId = 4;

void ExpectStats(const WebTransportSendStatsTracker& tracker,
                 quic::QuicStreamId stream_id,
                 uint64_t bytes_sent,
                 uint64_t bytes_acknowledged) {
  EXPECT_EQ(tracker.GetStats(stream_id),
            (WebTransportSendStreamStats{
                .bytes_sent = bytes_sent,
                .bytes_acknowledged = bytes_acknowledged,
            }));
}

TEST(WebTransportSendStatsTrackerTest, ClipsPreambleRanges) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 3);

  tracker.RecordSent(kStreamId, 0, 2);
  ExpectStats(tracker, kStreamId, 0, 0);

  tracker.RecordSent(kStreamId, 2, 6);
  ExpectStats(tracker, kStreamId, 5, 0);

  tracker.RecordAcknowledged(kStreamId, 0, 2);
  tracker.RecordAcknowledged(kStreamId, 2, 4);
  ExpectStats(tracker, kStreamId, 5, 3);

  tracker.RecordAcknowledged(kStreamId, 6, 2);
  ExpectStats(tracker, kStreamId, 5, 5);
}

TEST(WebTransportSendStatsTrackerTest,
     SingleAcknowledgementSpansPreambleAndApplicationData) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 3);
  tracker.RecordSent(kStreamId, 0, 10);
  ExpectStats(tracker, kStreamId, 7, 0);

  tracker.RecordAcknowledged(kStreamId, 0, 8);
  ExpectStats(tracker, kStreamId, 7, 5);
}

TEST(WebTransportSendStatsTrackerTest,
     DeduplicatesRetransmittedAndOverlappingAcknowledgements) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 0);
  tracker.RecordSent(kStreamId, 0, 10);

  tracker.RecordAcknowledged(kStreamId, 0, 6);
  tracker.RecordAcknowledged(kStreamId, 0, 6);
  tracker.RecordAcknowledged(kStreamId, 4, 6);
  ExpectStats(tracker, kStreamId, 10, 10);
}

TEST(WebTransportSendStatsTrackerTest,
     ReportsOnlyContiguousAcknowledgedPrefix) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 0);
  tracker.RecordSent(kStreamId, 0, 10);

  tracker.RecordAcknowledged(kStreamId, 5, 5);
  ExpectStats(tracker, kStreamId, 10, 0);

  tracker.RecordAcknowledged(kStreamId, 0, 3);
  ExpectStats(tracker, kStreamId, 10, 3);

  tracker.RecordAcknowledged(kStreamId, 3, 2);
  ExpectStats(tracker, kStreamId, 10, 10);
}

TEST(WebTransportSendStatsTrackerTest, ZeroLengthFinDoesNotChangeStats) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 2);
  tracker.RecordSent(kStreamId, 2, 4);
  tracker.RecordAcknowledged(kStreamId, 2, 4);

  // FIN-only stream frames have no data range.
  tracker.RecordSent(kStreamId, 6, 0);
  tracker.RecordAcknowledged(kStreamId, 6, 0);
  ExpectStats(tracker, kStreamId, 4, 4);
}

TEST(WebTransportSendStatsTrackerTest, TracksMultipleStreamsIndependently) {
  WebTransportSendStatsTracker tracker;
  constexpr quic::QuicStreamId kOtherStreamId = 8;
  tracker.RegisterStream(kStreamId, 0);
  tracker.RegisterStream(kOtherStreamId, 2);

  tracker.RecordSent(kStreamId, 0, 5);
  tracker.RecordSent(kOtherStreamId, 2, 9);
  tracker.RecordAcknowledged(kOtherStreamId, 2, 4);

  ExpectStats(tracker, kStreamId, 5, 0);
  ExpectStats(tracker, kOtherStreamId, 9, 4);
}

TEST(WebTransportSendStatsTrackerTest, UnregisterRemovesStreamState) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 0);
  tracker.RecordSent(kStreamId, 0, 5);
  tracker.UnregisterStream(kStreamId);

  EXPECT_FALSE(tracker.GetStats(kStreamId));

  tracker.RecordSent(kStreamId, 5, 5);
  tracker.RecordAcknowledged(kStreamId, 0, 10);
  EXPECT_FALSE(tracker.GetStats(kStreamId));
}

TEST(WebTransportSendStatsTrackerTest, SentBytesAreMonotonic) {
  WebTransportSendStatsTracker tracker;
  tracker.RegisterStream(kStreamId, 5);

  tracker.RecordSent(kStreamId, 5, 5);
  tracker.RecordSent(kStreamId, 5, 2);
  ExpectStats(tracker, kStreamId, 5, 0);

  tracker.RecordSent(kStreamId, 8, 4);
  ExpectStats(tracker, kStreamId, 7, 0);

  tracker.RecordSent(kStreamId, 0, 5);
  ExpectStats(tracker, kStreamId, 7, 0);
}

}  // namespace
}  // namespace net
