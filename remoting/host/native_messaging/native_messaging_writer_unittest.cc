// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_writer.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class NativeMessagingWriterTest : public testing::Test {
 public:
  NativeMessagingWriterTest();
  ~NativeMessagingWriterTest() override;

  void SetUp() override;

 protected:
  std::unique_ptr<NativeMessagingWriter> writer_;
  base::File read_file_;
  base::File write_file_;
};

NativeMessagingWriterTest::NativeMessagingWriterTest() = default;
NativeMessagingWriterTest::~NativeMessagingWriterTest() = default;

void NativeMessagingWriterTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  writer_ = std::make_unique<NativeMessagingWriter>(std::move(write_file_));
}

TEST_F(NativeMessagingWriterTest, GoodMessage) {
  base::Value::Dict dict;
  dict.Set("foo", 42);
  base::Value message(std::move(dict));
  EXPECT_TRUE(writer_->WriteMessage(message));

  // Read and verify header (platform's native endian).
  std::uint32_t body_length;
  auto header_read = read_file_.ReadAtCurrentPos(
      base::as_writable_byte_span(base::span_from_ref(body_length)));
  ASSERT_TRUE(header_read.has_value());
  ASSERT_EQ(sizeof(body_length), *header_read);

  std::string body_buffer(body_length, '\0');
  auto body_read =
      read_file_.ReadAtCurrentPos(base::as_writable_byte_span(body_buffer));
  ASSERT_TRUE(body_read.has_value());
  ASSERT_EQ(static_cast<size_t>(body_length), *body_read);

  base::Value::Dict written = base::test::ParseJsonDict(body_buffer);
  EXPECT_EQ(message, written);

  // Ensure no extra data: read returns zero or nullopt on EOF.
  writer_.reset();
  std::vector<uint8_t> eof_buffer(1);
  auto eof_read =
      read_file_.ReadAtCurrentPos(base::as_writable_byte_span(eof_buffer));
  EXPECT_EQ(0u, eof_read.value_or(0u));
}

TEST_F(NativeMessagingWriterTest, SecondMessage) {
  auto messages = std::to_array<base::Value>({
      base::Value(base::Value::Dict{}),
      base::Value(base::Value::Dict().Set("foo", 42)),
  });
  EXPECT_TRUE(writer_->WriteMessage(messages[0]));
  EXPECT_TRUE(writer_->WriteMessage(messages[1]));
  writer_.reset();

  for (size_t i = 0; i < messages.size(); ++i) {
    std::uint32_t length;
    auto header_read = read_file_.ReadAtCurrentPos(
        base::as_writable_byte_span(base::span_from_ref(length)));
    ASSERT_TRUE(header_read.has_value());
    ASSERT_EQ(sizeof(length), *header_read) << "i = " << i;

    std::string body_buffer(length, '\0');
    auto body_read =
        read_file_.ReadAtCurrentPos(base::as_writable_byte_span(body_buffer));
    ASSERT_TRUE(body_read.has_value());
    ASSERT_EQ(static_cast<size_t>(length), *body_read) << "i = " << i;

    // Verify message content.
    base::Value::Dict written = base::test::ParseJsonDict(body_buffer);
    EXPECT_EQ(messages[i], written);
  }
}

TEST_F(NativeMessagingWriterTest, FailedWrite) {
  // Close the read end so that writing fails immediately.
  read_file_.Close();

  base::Value message(base::Value::Dict{});
  EXPECT_FALSE(writer_->WriteMessage(message));
}

}  // namespace remoting
