// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/http_encoder.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

class HttpEncoderTest : public QuicTest {
 public:
  HttpEncoderTest() {}
  HttpEncoder encoder_;
};

TEST_F(HttpEncoderTest, SerializeDataFrameHeader) {
  DataFrame data;
  data.data = "Data!";
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeDataFrameHeader(data, &buffer);
  char output[] = {// length
                   0x05,
                   // type (DATA)
                   0x00};
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("DATA", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializeHeadersFrameHeader) {
  HeadersFrame headers;
  headers.headers = "Headers";
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeHeadersFrameHeader(headers, &buffer);
  char output[] = {// length
                   0x07,
                   // type (HEADERS)
                   0x01};
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("HEADERS", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializePriorityFrame) {
  PriorityFrame priority;
  priority.prioritized_type = REQUEST_STREAM;
  priority.dependency_type = REQUEST_STREAM;
  priority.exclusive = true;
  priority.prioritized_element_id = 0x03;
  priority.element_dependency_id = 0x04;
  priority.weight = 0xFF;
  char output[] = {// length
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

  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializePriorityFrame(priority, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("PRIORITY", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializeCancelPushFrame) {
  CancelPushFrame cancel_push;
  cancel_push.push_id = 0x01;
  char output[] = {// length
                   0x1,
                   // type (CANCEL_PUSH)
                   0x03,
                   // Push Id
                   0x01};
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeCancelPushFrame(cancel_push, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("CANCEL_PUSH", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializeSettingsFrame) {
  SettingsFrame settings;
  settings.values[3] = 2;
  settings.values[6] = 5;
  // clang-format off
  char output[] = {
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
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeSettingsFrame(settings, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("SETTINGS", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializePushPromiseFrameWithOnlyPushId) {
  PushPromiseFrame push_promise;
  push_promise.push_id = 0x01;
  push_promise.headers = "Headers";
  char output[] = {// length
                   0x8,
                   // type (PUSH_PROMISE)
                   0x05,
                   // Push Id
                   0x01};
  std::unique_ptr<char[]> buffer;
  uint64_t length =
      encoder_.SerializePushPromiseFrameWithOnlyPushId(push_promise, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("PUSH_PROMISE", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializeGoAwayFrame) {
  GoAwayFrame goaway;
  goaway.stream_id = 0x1;
  char output[] = {// length
                   0x1,
                   // type (GOAWAY)
                   0x07,
                   // StreamId
                   0x01};
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeGoAwayFrame(goaway, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("GOAWAY", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

TEST_F(HttpEncoderTest, SerializeMaxPushIdFrame) {
  MaxPushIdFrame max_push_id;
  max_push_id.push_id = 0x1;
  char output[] = {// length
                   0x1,
                   // type (MAX_PUSH_ID)
                   0x0D,
                   // Push Id
                   0x01};
  std::unique_ptr<char[]> buffer;
  uint64_t length = encoder_.SerializeMaxPushIdFrame(max_push_id, &buffer);
  EXPECT_EQ(QUIC_ARRAYSIZE(output), length);
  CompareCharArraysWithHexError("MAX_PUSH_ID", buffer.get(), length, output,
                                QUIC_ARRAYSIZE(output));
}

}  // namespace test
}  // namespace quic
