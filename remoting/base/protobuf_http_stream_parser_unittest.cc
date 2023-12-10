// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_stream_parser.h"

#include <memory>
#include <string>

#include "base/test/mock_callback.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_client_test_messages.pb.h"
#include "remoting/base/protobuf_http_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;

constexpr char kFirstTestMessage[] = "This is the first message.";
constexpr char kSecondTestMessage[] = "This is the second message.";
constexpr char kThirdTestMessage[] = "This is the third message.";

MATCHER(IsCancelStatus, "") {
  return arg.error_code() == ProtobufHttpStatus::Code::CANCELLED &&
         arg.error_message() == "Cancelled";
}

protobufhttpclient::StreamBody CreateDefaultStream() {
  protobufhttpclient::StreamBody stream_body;
  stream_body.add_messages(kFirstTestMessage);
  stream_body.add_messages(kSecondTestMessage);
  stream_body.add_messages(kThirdTestMessage);
  return stream_body;
}

std::string CreateDefaultStreamData() {
  return CreateDefaultStream().SerializeAsString();
}

void SplitStringInMiddle(const std::string& input,
                         std::string* str_1,
                         std::string* str_2) {
  size_t message_split_pos = input.size() / 2;
  *str_1 = input.substr(0, message_split_pos);
  *str_2 = input.substr(message_split_pos);
}

}  // namespace

class ProtobufHttpStreamParserTest : public testing::Test {
 protected:
  void ExpectReceiveDefaultStreamData();

  base::MockCallback<ProtobufHttpStreamParser::MessageCallback>
      message_callback_;
  base::MockCallback<ProtobufHttpStreamParser::StreamClosedCallback>
      stream_closed_callback_;
  ProtobufHttpStreamParser stream_parser_{message_callback_.Get(),
                                          stream_closed_callback_.Get()};
};

void ProtobufHttpStreamParserTest::ExpectReceiveDefaultStreamData() {
  EXPECT_CALL(message_callback_, Run(kFirstTestMessage));
  EXPECT_CALL(message_callback_, Run(kSecondTestMessage));
  EXPECT_CALL(message_callback_, Run(kThirdTestMessage));
}

TEST_F(ProtobufHttpStreamParserTest, ParseStreamBodyInOneShot) {
  ExpectReceiveDefaultStreamData();

  ASSERT_FALSE(stream_parser_.HasPendingData());
  stream_parser_.Append(CreateDefaultStreamData());
  ASSERT_FALSE(stream_parser_.HasPendingData());
}

TEST_F(ProtobufHttpStreamParserTest, ParseSplitStreamBody) {
  ExpectReceiveDefaultStreamData();

  std::string stream_data = CreateDefaultStreamData();
  std::string data_1, data_2;
  SplitStringInMiddle(stream_data, &data_1, &data_2);
  stream_parser_.Append(data_1);
  ASSERT_TRUE(stream_parser_.HasPendingData());
  stream_parser_.Append(data_2);
  ASSERT_FALSE(stream_parser_.HasPendingData());
}

TEST_F(ProtobufHttpStreamParserTest, CloseStreamWithCancelled) {
  EXPECT_CALL(stream_closed_callback_, Run(IsCancelStatus()));

  protobufhttpclient::StreamBody stream_body;
  stream_body.mutable_status()->set_code(
      static_cast<int>(ProtobufHttpStatus::Code::CANCELLED));
  stream_body.mutable_status()->set_message("Cancelled");
  stream_parser_.Append(stream_body.SerializeAsString());
}

TEST_F(ProtobufHttpStreamParserTest, ParseStreamBodyWithNoops_NoopsIgnored) {
  ExpectReceiveDefaultStreamData();

  protobufhttpclient::StreamBody stream_body = CreateDefaultStream();
  stream_body.add_noop("111111111111111");
  stream_body.add_noop("111111111111111");
  stream_body.add_noop("111111111111111");

  stream_parser_.Append(stream_body.SerializeAsString());
  ASSERT_FALSE(stream_parser_.HasPendingData());
}

TEST_F(ProtobufHttpStreamParserTest,
       ParseSplitStreamBodyWithOnlyNoops_NoopsIgnored) {
  protobufhttpclient::StreamBody stream_body;
  stream_body.add_noop("111111111111111");
  stream_body.add_noop("111111111111111");
  stream_body.add_noop("111111111111111");
  std::string stream_data = stream_body.SerializeAsString();
  std::string data_1, data_2;
  SplitStringInMiddle(stream_data, &data_1, &data_2);

  stream_parser_.Append(data_1);
  ASSERT_TRUE(stream_parser_.HasPendingData());
  stream_parser_.Append(data_2);
  ASSERT_FALSE(stream_parser_.HasPendingData());
}

TEST_F(ProtobufHttpStreamParserTest,
       ParseStreamBodyWithInvalidMessages_StreamIsClosed) {
  EXPECT_CALL(stream_closed_callback_, Run(_));

  protobufhttpclienttest::InvalidStreamBody stream_body;
  stream_body.set_messages(1);
  std::string stream_data = stream_body.SerializeAsString();
  stream_parser_.Append(stream_data);
}

TEST_F(ProtobufHttpStreamParserTest,
       ParseStreamBodyWithInvalidStatus_StreamIsClosed) {
  EXPECT_CALL(stream_closed_callback_, Run(_));

  protobufhttpclienttest::InvalidStreamBody stream_body;
  stream_body.set_status(2);
  std::string stream_data = stream_body.SerializeAsString();
  stream_parser_.Append(stream_data);
}

}  // namespace remoting
