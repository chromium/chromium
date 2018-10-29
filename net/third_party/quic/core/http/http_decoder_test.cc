// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/http_decoder.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_test.h"

using testing::InSequence;

namespace quic {

class MockVisitor : public HttpDecoder::Visitor {
 public:
  virtual ~MockVisitor() = default;

  // Called if an error is detected.
  MOCK_METHOD1(OnError, void(HttpDecoder* decoder));

  MOCK_METHOD1(OnPriorityFrame, void(const PriorityFrame& frame));
  MOCK_METHOD1(OnCancelPushFrame, void(const CancelPushFrame& frame));
  MOCK_METHOD1(OnMaxPushIdFrame, void(const MaxPushIdFrame& frame));
  MOCK_METHOD1(OnGoAwayFrame, void(const GoAwayFrame& frame));
  MOCK_METHOD1(OnSettingsFrame, void(const SettingsFrame& frame));

  MOCK_METHOD0(OnDataFrameStart, void());
  MOCK_METHOD1(OnDataFramePayload, void(QuicStringPiece payload));
  MOCK_METHOD0(OnDataFrameEnd, void());

  MOCK_METHOD0(OnHeadersFrameStart, void());
  MOCK_METHOD1(OnHeadersFramePayload, void(QuicStringPiece payload));
  MOCK_METHOD0(OnHeadersFrameEnd, void());

  MOCK_METHOD1(OnPushPromiseFrameStart, void(PushId push_id));
  MOCK_METHOD1(OnPushPromiseFramePayload, void(QuicStringPiece payload));
  MOCK_METHOD0(OnPushPromiseFrameEnd, void());
};

class HttpDecoderTest : public QuicTest {
 public:
  HttpDecoderTest() { decoder_.set_visitor(&visitor_); }
  HttpDecoder decoder_;
  testing::StrictMock<MockVisitor> visitor_;
};

TEST_F(HttpDecoderTest, InitialState) {
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, ReservedFramesNoPayload) {
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    char input[] = {// length
                    0x00,
                    // type
                    type};

    EXPECT_EQ(2u, decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input))) << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
  }
}

TEST_F(HttpDecoderTest, ReservedFramesSmallPayload) {
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    const uint8_t payload_size = 50;
    char input[payload_size + 2] = {// length
                                    payload_size,
                                    // type
                                    type};

    EXPECT_EQ(QUIC_ARRAYSIZE(input),
              decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)))
        << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
  }
}

TEST_F(HttpDecoderTest, ReservedFramesLargePayload) {
  for (int n = 0; n < 8; ++n) {
    const uint8_t type = 0xB + 0x1F * n;
    const size_t payload_size = 256;
    char input[payload_size + 3] = {// length
                                    0x40 + 0x01, 0x00,
                                    // type
                                    type};

    EXPECT_EQ(QUIC_ARRAYSIZE(input),
              decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)))
        << n;
    EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
    ASSERT_EQ("", decoder_.error_detail());
  }
}

TEST_F(HttpDecoderTest, CancelPush) {
  char input[] = {// length
                  0x1,
                  // type (CANCEL_PUSH)
                  0x03,
                  // Push Id
                  0x01};

  // Process the full frame.
  EXPECT_CALL(visitor_, OnCancelPushFrame(CancelPushFrame({1})));
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnCancelPushFrame(CancelPushFrame({1})));
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PushPromiseFrame) {
  char input[] = {// length
                  0x8,
                  // type (PUSH_PROMISE)
                  0x05,
                  // Push Id
                  0x01,
                  // Header Block
                  'H', 'e', 'a', 'd', 'e', 'r', 's'};

  // Process the full frame.
  InSequence s;
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("Headers")));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnPushPromiseFrameStart(1));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("H")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("d")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("r")));
  EXPECT_CALL(visitor_, OnPushPromiseFramePayload(QuicStringPiece("s")));
  EXPECT_CALL(visitor_, OnPushPromiseFrameEnd());
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MaxPushId) {
  char input[] = {// length
                  0x1,
                  // type (MAX_PUSH_ID)
                  0x0D,
                  // Push Id
                  0x01};

  // Process the full frame.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame(MaxPushIdFrame({1})));
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame(MaxPushIdFrame({1})));
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PriorityFrame) {
  char input[] = {// length
                  0x4,
                  // type (PRIORITY)
                  0x2,
                  // request stream, request stream, exclusive
                  0x01,
                  // prioritized_element_id
                  0x03,
                  // element_dependency_id
                  0x04,
                  // weight
                  0xFF};

  PriorityFrame frame;
  frame.prioritized_type = REQUEST_STREAM;
  frame.dependency_type = REQUEST_STREAM;
  frame.exclusive = true;
  frame.prioritized_element_id = 0x03;
  frame.element_dependency_id = 0x04;
  frame.weight = 0xFF;

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPriorityFrame(frame));
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  /*
  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnPriorityFrame(frame));
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
  */
}

TEST_F(HttpDecoderTest, SettingsFrame) {
  // clang-format off
  char input[] = {
      // length
      0x06,
      // type (SETTINGS)
      0x04,
      // identifier (SETTINGS_NUM_PLACEHOLDERS)
      0x00,
      0x03,
      // content
      0x02,
      // identifier (SETTINGS_MAX_HEADER_LIST_SIZE)
      0x00,
      0x06,
      // content
      0x05,
  };
  // clang-format on

  SettingsFrame frame;
  frame.values[3] = 2;
  frame.values[6] = 5;

  // Process the full frame.
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, DataFrame) {
  char input[] = {// length
                  0x05,
                  // type (DATA)
                  0x00,
                  // data
                  'D', 'a', 't', 'a', '!'};

  // Process the full frame.
  InSequence s;
  EXPECT_CALL(visitor_, OnDataFrameStart());
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("Data!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnDataFrameStart());
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("D")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("t")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(QuicStringPiece("!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, GoAway) {
  char input[] = {// length
                  0x1,
                  // type (GOAWAY)
                  0x07,
                  // StreamId
                  0x01};

  // Process the full frame.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, HeadersFrame) {
  char input[] = {// length
                  0x07,
                  // type (HEADERS)
                  0x01,
                  // headers
                  'H', 'e', 'a', 'd', 'e', 'r', 's'};

  // Process the full frame.
  InSequence s;
  EXPECT_CALL(visitor_, OnHeadersFrameStart());
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("Headers")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  EXPECT_EQ(QUIC_ARRAYSIZE(input),
            decoder_.ProcessInput(input, QUIC_ARRAYSIZE(input)));
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incremently.
  EXPECT_CALL(visitor_, OnHeadersFrameStart());
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("H")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("a")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("d")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("r")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(QuicStringPiece("s")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  for (char c : input) {
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }
  EXPECT_EQ(QUIC_NO_ERROR, decoder_.error());
  EXPECT_EQ("", decoder_.error_detail());
}

}  // namespace quic
