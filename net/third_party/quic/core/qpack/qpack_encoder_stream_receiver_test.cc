// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_encoder_stream_receiver.h"

#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockDelegate : public QpackEncoderStreamReceiver::Delegate {
 public:
  ~MockDelegate() override = default;

  MOCK_METHOD3(OnInsertWithNameReference,
               void(bool is_static,
                    uint64_t name_index,
                    QuicStringPiece value));
  MOCK_METHOD2(OnInsertWithoutNameReference,
               void(QuicStringPiece name, QuicStringPiece value));
  MOCK_METHOD1(OnDuplicate, void(uint64_t index));
  MOCK_METHOD1(OnDynamicTableSizeUpdate, void(uint64_t max_size));
  MOCK_METHOD1(OnErrorDetected, void(QuicStringPiece error_message));
};

class QpackEncoderStreamReceiverTest : public QuicTest {
 protected:
  QpackEncoderStreamReceiverTest() : stream_(&delegate_) {}

  void Decode(QuicStringPiece data) { stream_.Decode(data); }
  StrictMock<MockDelegate>* delegate() { return &delegate_; }

 private:
  QpackEncoderStreamReceiver stream_;
  StrictMock<MockDelegate> delegate_;
};

TEST_F(QpackEncoderStreamReceiverTest, InsertWithNameReference) {
  // Static, index fits in prefix, empty value.
  EXPECT_CALL(*delegate(), OnInsertWithNameReference(true, 5, Eq("")));
  // Static, index fits in prefix, Huffman encoded value.
  EXPECT_CALL(*delegate(), OnInsertWithNameReference(true, 2, Eq("foo")));
  // Not static, index does not fit in prefix, not Huffman encoded value.
  EXPECT_CALL(*delegate(), OnInsertWithNameReference(false, 137, Eq("bar")));
  // Value length does not fit in prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  EXPECT_CALL(*delegate(),
              OnInsertWithNameReference(false, 42, Eq(QuicString(127, 'Z'))));

  Decode(QuicTextUtils::HexDecode(
      "c500"
      "c28294e7"
      "bf4a03626172"
      "aa7f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
}

TEST_F(QpackEncoderStreamReceiverTest, InsertWithNameReferenceIndexTooLarge) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("Encoded integer too large.")));

  Decode(QuicTextUtils::HexDecode("bfffffffffffffffffffffff"));
}

TEST_F(QpackEncoderStreamReceiverTest, InsertWithNameReferenceValueTooLong) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("ValueLen too large.")));

  Decode(QuicTextUtils::HexDecode("c57fffffffffffffffffffff"));
}

TEST_F(QpackEncoderStreamReceiverTest, InsertWithoutNameReference) {
  // Empty name and value.
  EXPECT_CALL(*delegate(), OnInsertWithoutNameReference(Eq(""), Eq("")));
  // Huffman encoded short strings.
  EXPECT_CALL(*delegate(), OnInsertWithoutNameReference(Eq("bar"), Eq("bar")));
  // Not Huffman encoded short strings.
  EXPECT_CALL(*delegate(), OnInsertWithoutNameReference(Eq("foo"), Eq("foo")));
  // Not Huffman encoded long strings; length does not fit on prefix.
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  EXPECT_CALL(*delegate(),
              OnInsertWithoutNameReference(Eq(QuicString(31, 'Z')),
                                           Eq(QuicString(127, 'Z'))));

  Decode(QuicTextUtils::HexDecode(
      "4000"
      "4362617203626172"
      "6294e78294e7"
      "5f005a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a7f005a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"));
}

TEST_F(QpackEncoderStreamReceiverTest, InsertWithoutNameReferenceNameTooLong) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("Encoded integer too large.")));

  Decode(QuicTextUtils::HexDecode("5fffffffffffffffffffff"));
}

TEST_F(QpackEncoderStreamReceiverTest, InsertWithoutNameReferenceValueTooLong) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("ValueLen too large.")));

  Decode(QuicTextUtils::HexDecode("436261727fffffffffffffffffffff"));
}

TEST_F(QpackEncoderStreamReceiverTest, Duplicate) {
  // Small index fits in prefix.
  EXPECT_CALL(*delegate(), OnDuplicate(17));
  // Large index requires two extension bytes.
  EXPECT_CALL(*delegate(), OnDuplicate(500));

  Decode(QuicTextUtils::HexDecode("111fd503"));
}

TEST_F(QpackEncoderStreamReceiverTest, DuplicateIndexTooLarge) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("Encoded integer too large.")));

  Decode(QuicTextUtils::HexDecode("1fffffffffffffffffffff"));
}

TEST_F(QpackEncoderStreamReceiverTest, DynamicTableSizeUpdate) {
  // Small max size fits in prefix.
  EXPECT_CALL(*delegate(), OnDynamicTableSizeUpdate(17));
  // Large max size requires two extension bytes.
  EXPECT_CALL(*delegate(), OnDynamicTableSizeUpdate(500));

  Decode(QuicTextUtils::HexDecode("313fd503"));
}

TEST_F(QpackEncoderStreamReceiverTest, DynamicTableSizeUpdateMaxSizeTooLarge) {
  EXPECT_CALL(*delegate(), OnErrorDetected(Eq("Encoded integer too large.")));

  Decode(QuicTextUtils::HexDecode("3fffffffffffffffffffff"));
}

}  // namespace
}  // namespace test
}  // namespace quic
