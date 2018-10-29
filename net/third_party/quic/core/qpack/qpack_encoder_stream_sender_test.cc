// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder_stream_sender.h"

#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {
namespace test {
namespace {

class TestSendingDelegate : public QpackEncoderStreamSender::Delegate {
 public:
  ~TestSendingDelegate() override = default;

  void Write(QuicStringPiece data) override {
    EXPECT_FALSE(data.empty());
    buffer_.append(data.data(), data.size());
  }

  const QuicString& buffer() { return buffer_; }

 private:
  QuicString buffer_;
};

class QpackEncoderStreamSenderTest : public QuicTest {
 protected:
  QpackEncoderStreamSenderTest() : stream_(&delegate_) {}

  QpackEncoderStreamSender* stream() { return &stream_; }
  const QuicString& buffer() { return delegate_.buffer(); }

 private:
  TestSendingDelegate delegate_;
  QpackEncoderStreamSender stream_;
};

TEST_F(QpackEncoderStreamSenderTest, InsertWithNameReference) {
  // Static, index fits in prefix, empty value.
  stream()->SendInsertWithNameReference(true, 5, "");
  // Static, index fits in prefix, Huffman encoded value.
  stream()->SendInsertWithNameReference(true, 2, "foo");
  // Not static, index does not fit in prefix, not Huffman encoded value.
  stream()->SendInsertWithNameReference(false, 137, "bar");
  // Value length does not fit in prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  stream()->SendInsertWithNameReference(false, 42, QuicString(127, 'Z'));

  EXPECT_EQ(
      QuicTextUtils::HexDecode(
          "c500"
          "c28294e7"
          "bf4a03626172"
          "aa7f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"),
      buffer());
}

TEST_F(QpackEncoderStreamSenderTest, InsertWithoutNameReference) {
  // Empty name and value.
  stream()->SendInsertWithoutNameReference("", "");
  // Huffman encoded short strings.
  stream()->SendInsertWithoutNameReference("bar", "bar");
  // Not Huffman encoded short strings.
  stream()->SendInsertWithoutNameReference("foo", "foo");
  // Not Huffman encoded long strings; length does not fit on prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  stream()->SendInsertWithoutNameReference(QuicString(31, 'Z'),
                                           QuicString(127, 'Z'));

  EXPECT_EQ(
      QuicTextUtils::HexDecode(
          "4000"
          "4362617203626172"
          "6294e78294e7"
          "5f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a7f"
          "005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
          "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"),
      buffer());
}

TEST_F(QpackEncoderStreamSenderTest, Duplicate) {
  // Small index fits in prefix.
  stream()->SendDuplicate(17);
  // Large index requires two extension bytes.
  stream()->SendDuplicate(500);

  EXPECT_EQ(QuicTextUtils::HexDecode("111fd503"), buffer());
}

TEST_F(QpackEncoderStreamSenderTest, DynamicTableSizeUpdate) {
  // Small max size fits in prefix.
  stream()->SendDynamicTableSizeUpdate(17);
  // Large max size requires two extension bytes.
  stream()->SendDynamicTableSizeUpdate(500);

  EXPECT_EQ(QuicTextUtils::HexDecode("313fd503"), buffer());
}

}  // namespace
}  // namespace test
}  // namespace quic
