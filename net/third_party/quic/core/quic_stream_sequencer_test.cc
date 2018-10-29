// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_stream_sequencer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "net/third_party/quic/core/quic_stream.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_sequencer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;

namespace quic {
namespace test {

class MockStream : public QuicStream {
 public:
  MockStream(QuicSession* session, QuicStreamId id)
      : QuicStream(id, session, /*is_static=*/false, BIDIRECTIONAL) {}

  MOCK_METHOD0(OnFinRead, void());
  MOCK_METHOD0(OnDataAvailable, void());
  MOCK_METHOD2(CloseConnectionWithDetails,
               void(QuicErrorCode error, const QuicString& details));
  MOCK_METHOD1(Reset, void(QuicRstStreamErrorCode error));
  MOCK_METHOD0(OnCanWrite, void());

  const QuicSocketAddress& PeerAddressOfLatestPacket() const override {
    return peer_address_;
  }

 protected:
  QuicSocketAddress peer_address_ =
      QuicSocketAddress(QuicIpAddress::Any4(), 65535);
};

namespace {

static const char kPayload[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

class QuicStreamSequencerTest : public QuicTest {
 public:
  void ConsumeData(size_t num_bytes) {
    char buffer[1024];
    ASSERT_GT(QUIC_ARRAYSIZE(buffer), num_bytes);
    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = num_bytes;
    ASSERT_EQ(static_cast<int>(num_bytes), sequencer_->Readv(&iov, 1));
  }

 protected:
  QuicStreamSequencerTest()
      : connection_(new MockQuicConnection(&helper_,
                                           &alarm_factory_,
                                           Perspective::IS_CLIENT)),
        session_(connection_),
        stream_(&session_, 1),
        sequencer_(new QuicStreamSequencer(&stream_)) {}

  // Verify that the data in first region match with the expected[0].
  bool VerifyReadableRegion(const std::vector<QuicString>& expected) {
    iovec iovecs[1];
    if (sequencer_->GetReadableRegions(iovecs, 1)) {
      return (VerifyIovecs(iovecs, 1, std::vector<QuicString>{expected[0]}));
    }
    return false;
  }

  // Verify that the data in each of currently readable regions match with each
  // item given in |expected|.
  bool VerifyReadableRegions(const std::vector<QuicString>& expected) {
    iovec iovecs[5];
    size_t num_iovecs =
        sequencer_->GetReadableRegions(iovecs, QUIC_ARRAYSIZE(iovecs));
    return VerifyReadableRegion(expected) &&
           VerifyIovecs(iovecs, num_iovecs, expected);
  }

  bool VerifyIovecs(iovec* iovecs,
                    size_t num_iovecs,
                    const std::vector<QuicString>& expected) {
    int start_position = 0;
    for (size_t i = 0; i < num_iovecs; ++i) {
      if (!VerifyIovec(iovecs[i],
                       expected[0].substr(start_position, iovecs[i].iov_len))) {
        return false;
      }
      start_position += iovecs[i].iov_len;
    }
    return true;
  }

  bool VerifyIovec(const iovec& iovec, QuicStringPiece expected) {
    if (iovec.iov_len != expected.length()) {
      QUIC_LOG(ERROR) << "Invalid length: " << iovec.iov_len << " vs "
                      << expected.length();
      return false;
    }
    if (memcmp(iovec.iov_base, expected.data(), expected.length()) != 0) {
      QUIC_LOG(ERROR) << "Invalid data: " << static_cast<char*>(iovec.iov_base)
                      << " vs " << expected;
      return false;
    }
    return true;
  }

  void OnFinFrame(QuicStreamOffset byte_offset, const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data_buffer = data;
    frame.data_length = strlen(data);
    frame.fin = true;
    sequencer_->OnStreamFrame(frame);
  }

  void OnFrame(QuicStreamOffset byte_offset, const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data_buffer = data;
    frame.data_length = strlen(data);
    frame.fin = false;
    sequencer_->OnStreamFrame(frame);
  }

  size_t NumBufferedBytes() {
    return QuicStreamSequencerPeer::GetNumBufferedBytes(sequencer_.get());
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  MockQuicSpdySession session_;
  testing::StrictMock<MockStream> stream_;
  std::unique_ptr<QuicStreamSequencer> sequencer_;
};

// TODO(rch): reorder these tests so they build on each other.

TEST_F(QuicStreamSequencerTest, RejectOldFrame) {
  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));

  OnFrame(0, "abc");

  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(3u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(3u, stream_.flow_controller()->bytes_consumed());
  // Ignore this - it matches a past packet number and we should not see it
  // again.
  OnFrame(0, "def");
  EXPECT_EQ(0u, NumBufferedBytes());
}

TEST_F(QuicStreamSequencerTest, RejectBufferedFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());

  // Ignore this - it matches a buffered frame.
  // Right now there's no checking that the payload is consistent.
  OnFrame(0, "def");
  EXPECT_EQ(3u, NumBufferedBytes());
}

TEST_F(QuicStreamSequencerTest, FullFrameConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));

  OnFrame(0, "abc");
  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(3u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, BlockedThenFullFrameConsumed) {
  sequencer_->SetBlockedUntilFlush();

  OnFrame(0, "abc");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));
  sequencer_->SetUnblocked();
  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(3u, sequencer_->NumBytesConsumed());

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFinFrame(3, "def");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, BlockedThenFullFrameAndFinConsumed) {
  sequencer_->SetBlockedUntilFlush();

  OnFinFrame(0, "abc");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));
  EXPECT_FALSE(sequencer_->IsClosed());
  sequencer_->SetUnblocked();
  EXPECT_TRUE(sequencer_->IsClosed());
  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(3u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFrame) {
  EXPECT_CALL(stream_,
              CloseConnectionWithDetails(QUIC_EMPTY_STREAM_FRAME_NO_FIN, _));
  OnFrame(0, "");
  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFinFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());
  OnFinFrame(0, "");
  EXPECT_EQ(0u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, PartialFrameConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(2);
  }));

  OnFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedBytes());
  EXPECT_EQ(2u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, NextxFrameNotConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, FutureFrameNotProcessed) {
  OnFrame(3, "abc");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFrameProcessed) {
  // Buffer the first
  OnFrame(6, "ghi");
  EXPECT_EQ(3u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(3u, sequencer_->NumBytesBuffered());
  // Buffer the second
  OnFrame(3, "def");
  EXPECT_EQ(6u, NumBufferedBytes());
  EXPECT_EQ(0u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(6u, sequencer_->NumBytesBuffered());

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(9);
  }));

  // Now process all of them at once.
  OnFrame(0, "abc");
  EXPECT_EQ(9u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());

  EXPECT_EQ(0u, NumBufferedBytes());
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseOrdered) {
  InSequence s;

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));
  OnFinFrame(0, "abc");

  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseUnorderedWithFlush) {
  OnFinFrame(6, "");
  EXPECT_EQ(6u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  OnFrame(3, "def");
  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(6);
  }));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFrame(0, "abc");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, BasicHalfUnordered) {
  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, OnDataAvailable()).WillOnce(testing::Invoke([this]() {
    ConsumeData(3);
  }));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFrame(0, "abc");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, TerminateWithReadv) {
  char buffer[3];

  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_FALSE(sequencer_->IsClosed());

  EXPECT_CALL(stream_, OnDataAvailable());
  OnFrame(0, "abc");

  iovec iov = {&buffer[0], 3};
  int bytes_read = sequencer_->Readv(&iov, 1);
  EXPECT_EQ(3, bytes_read);
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, MutipleOffsets) {
  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, Reset(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  OnFinFrame(5, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, Reset(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  OnFinFrame(1, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));
}

class QuicSequencerRandomTest : public QuicStreamSequencerTest {
 public:
  typedef std::pair<int, QuicString> Frame;
  typedef std::vector<Frame> FrameList;

  void CreateFrames() {
    int payload_size = QUIC_ARRAYSIZE(kPayload) - 1;
    int remaining_payload = payload_size;
    while (remaining_payload != 0) {
      int size = std::min(OneToN(6), remaining_payload);
      int index = payload_size - remaining_payload;
      list_.push_back(
          std::make_pair(index, QuicString(kPayload + index, size)));
      remaining_payload -= size;
    }
  }

  QuicSequencerRandomTest() {
    uint64_t seed = QuicRandom::GetInstance()->RandUint64();
    QUIC_LOG(INFO) << "**** The current seed is " << seed << " ****";
    random_.set_seed(seed);

    CreateFrames();
  }

  int OneToN(int n) { return random_.RandUint64() % n + 1; }

  void ReadAvailableData() {
    // Read all available data
    char output[QUIC_ARRAYSIZE(kPayload) + 1];
    iovec iov;
    iov.iov_base = output;
    iov.iov_len = QUIC_ARRAYSIZE(output);
    int bytes_read = sequencer_->Readv(&iov, 1);
    EXPECT_NE(0, bytes_read);
    output_.append(output, bytes_read);
  }

  QuicString output_;
  // Data which peek at using GetReadableRegion if we back up.
  QuicString peeked_;
  SimpleRandom random_;
  FrameList list_;
};

// All frames are processed as soon as we have sequential data.
// Infinite buffering, so all frames are acked right away.
TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingNoBackup) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable())
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke(this, &QuicSequencerRandomTest::ReadAvailableData));

  while (!list_.empty()) {
    int index = OneToN(list_.size()) - 1;
    QUIC_LOG(ERROR) << "Sending index " << index << " " << list_[index].second;
    OnFrame(list_[index].first, list_[index].second.data());

    list_.erase(list_.begin() + index);
  }

  ASSERT_EQ(QUIC_ARRAYSIZE(kPayload) - 1, output_.size());
  EXPECT_EQ(kPayload, output_);
}

TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingBackup) {
  char buffer[10];
  iovec iov[2];
  iov[0].iov_base = &buffer[0];
  iov[0].iov_len = 5;
  iov[1].iov_base = &buffer[5];
  iov[1].iov_len = 5;

  EXPECT_CALL(stream_, OnDataAvailable()).Times(AnyNumber());

  while (output_.size() != QUIC_ARRAYSIZE(kPayload) - 1) {
    if (!list_.empty() && OneToN(2) == 1) {  // Send data
      int index = OneToN(list_.size()) - 1;
      OnFrame(list_[index].first, list_[index].second.data());
      list_.erase(list_.begin() + index);
    } else {  // Read data
      bool has_bytes = sequencer_->HasBytesToRead();
      iovec peek_iov[20];
      int iovs_peeked = sequencer_->GetReadableRegions(peek_iov, 20);
      if (has_bytes) {
        ASSERT_LT(0, iovs_peeked);
        ASSERT_TRUE(sequencer_->GetReadableRegion(peek_iov));
      } else {
        ASSERT_EQ(0, iovs_peeked);
        ASSERT_FALSE(sequencer_->GetReadableRegion(peek_iov));
      }
      int total_bytes_to_peek = QUIC_ARRAYSIZE(buffer);
      for (int i = 0; i < iovs_peeked; ++i) {
        int bytes_to_peek =
            std::min<int>(peek_iov[i].iov_len, total_bytes_to_peek);
        peeked_.append(static_cast<char*>(peek_iov[i].iov_base), bytes_to_peek);
        total_bytes_to_peek -= bytes_to_peek;
        if (total_bytes_to_peek == 0) {
          break;
        }
      }
      int bytes_read = sequencer_->Readv(iov, 2);
      output_.append(buffer, bytes_read);
      ASSERT_EQ(output_.size(), peeked_.size());
    }
  }
  EXPECT_EQ(QuicString(kPayload), output_);
  EXPECT_EQ(QuicString(kPayload), peeked_);
}

// Same as above, just using a different method for reading.
TEST_F(QuicStreamSequencerTest, MarkConsumed) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(3, "def");
  OnFrame(6, "ghi");

  // abcdefghi buffered.
  EXPECT_EQ(9u, sequencer_->NumBytesBuffered());

  // Peek into the data.
  std::vector<QuicString> expected = {"abcdefghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected));

  // Consume 1 byte.
  sequencer_->MarkConsumed(1);
  EXPECT_EQ(1u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  std::vector<QuicString> expected2 = {"bcdefghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected2));
  EXPECT_EQ(8u, sequencer_->NumBytesBuffered());

  // Consume 2 bytes.
  sequencer_->MarkConsumed(2);
  EXPECT_EQ(3u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  std::vector<QuicString> expected3 = {"defghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected3));
  EXPECT_EQ(6u, sequencer_->NumBytesBuffered());

  // Consume 5 bytes.
  sequencer_->MarkConsumed(5);
  EXPECT_EQ(8u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  std::vector<QuicString> expected4{"i"};
  ASSERT_TRUE(VerifyReadableRegions(expected4));
  EXPECT_EQ(1u, sequencer_->NumBytesBuffered());
}

TEST_F(QuicStreamSequencerTest, MarkConsumedError) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(9, "jklmnopqrstuvwxyz");

  // Peek into the data.  Only the first chunk should be readable because of the
  // missing data.
  std::vector<QuicString> expected{"abc"};
  ASSERT_TRUE(VerifyReadableRegions(expected));

  // Now, attempt to mark consumed more data than was readable and expect the
  // stream to be closed.
  EXPECT_CALL(stream_, Reset(QUIC_ERROR_PROCESSING_STREAM));
  EXPECT_QUIC_BUG(sequencer_->MarkConsumed(4),
                  "Invalid argument to MarkConsumed."
                  " expect to consume: 4, but not enough bytes available.");
}

TEST_F(QuicStreamSequencerTest, MarkConsumedWithMissingPacket) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(3, "def");
  // Missing packet: 6, ghi.
  OnFrame(9, "jkl");

  std::vector<QuicString> expected = {"abcdef"};
  ASSERT_TRUE(VerifyReadableRegions(expected));

  sequencer_->MarkConsumed(6);
}

TEST_F(QuicStreamSequencerTest, OverlappingFramesReceived) {
  // The peer should never send us non-identical stream frames which contain
  // overlapping byte ranges - if they do, we close the connection.
  QuicStreamId id =
      QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0);

  QuicStreamFrame frame1(id, false, 1, QuicStringPiece("hello"));
  sequencer_->OnStreamFrame(frame1);

  QuicStreamFrame frame2(id, false, 2, QuicStringPiece("hello"));
  EXPECT_CALL(stream_,
              CloseConnectionWithDetails(QUIC_OVERLAPPING_STREAM_DATA, _))
      .Times(0);
  sequencer_->OnStreamFrame(frame2);
}

TEST_F(QuicStreamSequencerTest, DataAvailableOnOverlappingFrames) {
  QuicStreamId id =
      QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0);
  const QuicString data(1000, '.');

  // Received [0, 1000).
  QuicStreamFrame frame1(id, false, 0, data);
  EXPECT_CALL(stream_, OnDataAvailable());
  sequencer_->OnStreamFrame(frame1);
  // Consume [0, 500).
  QuicStreamSequencerTest::ConsumeData(500);
  EXPECT_EQ(500u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(500u, sequencer_->NumBytesBuffered());

  // Received [500, 1500).
  QuicStreamFrame frame2(id, false, 500, data);
  // Do not call OnDataAvailable as there are readable bytes left in the buffer.
  EXPECT_CALL(stream_, OnDataAvailable()).Times(0);
  sequencer_->OnStreamFrame(frame2);
  // Consume [1000, 1500).
  QuicStreamSequencerTest::ConsumeData(1000);
  EXPECT_EQ(1500u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());

  // Received [1498, 1503).
  QuicStreamFrame frame3(id, false, 1498, QuicStringPiece("hello"));
  EXPECT_CALL(stream_, OnDataAvailable());
  sequencer_->OnStreamFrame(frame3);
  QuicStreamSequencerTest::ConsumeData(3);
  EXPECT_EQ(1503u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());

  // Received [1000, 1005).
  QuicStreamFrame frame4(id, false, 1000, QuicStringPiece("hello"));
  EXPECT_CALL(stream_, OnDataAvailable()).Times(0);
  sequencer_->OnStreamFrame(frame4);
  EXPECT_EQ(1503u, sequencer_->NumBytesConsumed());
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());
}

TEST_F(QuicStreamSequencerTest, OnDataAvailableWhenReadableBytesIncrease) {
  sequencer_->set_level_triggered(true);
  QuicStreamId id =
      QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0);

  // Received [0, 5).
  QuicStreamFrame frame1(id, false, 0, "hello");
  EXPECT_CALL(stream_, OnDataAvailable());
  sequencer_->OnStreamFrame(frame1);
  EXPECT_EQ(5u, sequencer_->NumBytesBuffered());

  // Without consuming the buffer bytes, continue receiving [5, 11).
  QuicStreamFrame frame2(id, false, 5, " world");
  // OnDataAvailable should still be called because there are more data to read.
  EXPECT_CALL(stream_, OnDataAvailable());
  sequencer_->OnStreamFrame(frame2);
  EXPECT_EQ(11u, sequencer_->NumBytesBuffered());

  // Without consuming the buffer bytes, continue receiving [12, 13).
  QuicStreamFrame frame3(id, false, 5, "a");
  // OnDataAvailable shouldn't be called becasue there are still only 11 bytes
  // available.
  EXPECT_CALL(stream_, OnDataAvailable()).Times(0);
  sequencer_->OnStreamFrame(frame3);
  EXPECT_EQ(11u, sequencer_->NumBytesBuffered());
}

TEST_F(QuicStreamSequencerTest, ReadSingleFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());
  OnFrame(0u, "abc");
  QuicString actual;
  sequencer_->Read(&actual);
  EXPECT_EQ("abc", actual);
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());
  EXPECT_EQ(3u, stream_.flow_controller()->bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, ReadMultipleFramesWithMissingFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());
  OnFrame(0u, "abc");
  OnFrame(3u, "def");
  OnFrame(6u, "ghi");
  OnFrame(10u, "xyz");  // Byte 9 is missing.
  QuicString actual;
  sequencer_->Read(&actual);
  EXPECT_EQ("abcdefghi", actual);
  EXPECT_EQ(3u, sequencer_->NumBytesBuffered());
  EXPECT_EQ(9u, stream_.flow_controller()->bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, ReadAndAppendToString) {
  EXPECT_CALL(stream_, OnDataAvailable());
  OnFrame(0u, "def");
  OnFrame(3u, "ghi");
  QuicString actual = "abc";
  sequencer_->Read(&actual);
  EXPECT_EQ("abcdefghi", actual);
  EXPECT_EQ(0u, sequencer_->NumBytesBuffered());
  EXPECT_EQ(6u, stream_.flow_controller()->bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, StopReading) {
  EXPECT_CALL(stream_, OnDataAvailable()).Times(0);
  EXPECT_CALL(stream_, OnFinRead());

  sequencer_->StopReading();

  OnFrame(0u, "abc");
  OnFrame(3u, "def");
  OnFinFrame(6u, "ghi");
}

TEST_F(QuicStreamSequencerTest, StopReadingWithLevelTriggered) {
  if (GetQuicReloadableFlag(quic_stop_reading_when_level_triggered)) {
    EXPECT_CALL(stream_, OnDataAvailable()).Times(0);
    EXPECT_CALL(stream_, OnFinRead());
  } else {
    EXPECT_CALL(stream_, OnDataAvailable()).Times(3);
  }

  sequencer_->set_level_triggered(true);
  sequencer_->StopReading();

  OnFrame(0u, "abc");
  OnFrame(3u, "def");
  OnFinFrame(6u, "ghi");
}

}  // namespace
}  // namespace test
}  // namespace quic
