// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_decoder.h"

#include "base/logging.h"
#include "net/third_party/quic/core/qpack/qpack_decoder_test_utils.h"
#include "net/third_party/quic/core/qpack/qpack_test_utils.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/spdy/core/spdy_header_block.h"

using ::testing::StrictMock;
using ::testing::Values;

namespace quic {
namespace test {
namespace {

class MockHeadersHandler : public QpackDecoder::HeadersHandlerInterface {
 public:
  MockHeadersHandler() = default;
  MockHeadersHandler(const MockHeadersHandler&) = delete;
  MockHeadersHandler& operator=(const MockHeadersHandler&) = delete;
  ~MockHeadersHandler() override = default;

  MOCK_METHOD2(OnHeaderDecoded,
               void(QuicStringPiece name, QuicStringPiece value));
  MOCK_METHOD0(OnDecodingCompleted, void());
  MOCK_METHOD1(OnDecodingErrorDetected, void(QuicStringPiece error_message));
};

class QpackDecoderTest : public QuicTestWithParam<FragmentMode> {
 public:
  QpackDecoderTest() : fragment_mode_(GetParam()) {}

  void Decode(QuicStringPiece data) {
    QpackDecode(&handler_, FragmentModeToFragmentSizeGenerator(fragment_mode_),
                data);
  }

 protected:
  StrictMock<MockHeadersHandler> handler_;

 private:
  const FragmentMode fragment_mode_;
};

INSTANTIATE_TEST_CASE_P(,
                        QpackDecoderTest,
                        Values(FragmentMode::kSingleChunk,
                               FragmentMode::kOctetByOctet));

TEST_P(QpackDecoderTest, NotStarted) {
  EXPECT_CALL(handler_, OnDecodingCompleted());

  QpackDecoder decoder;
  auto progressive_decoder = decoder.DecodeHeaderBlock(&handler_);
  progressive_decoder->EndHeaderBlock();
}

TEST_P(QpackDecoderTest, Empty) {
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode("");
}

TEST_P(QpackDecoderTest, EmptyName) {
  EXPECT_CALL(handler_,
              OnHeaderDecoded(QuicStringPiece(""), QuicStringPiece("foo")));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode("2003666f6f"));
}

TEST_P(QpackDecoderTest, EmptyValue) {
  EXPECT_CALL(handler_,
              OnHeaderDecoded(QuicStringPiece("foo"), QuicStringPiece("")));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode("23666f6f00"));
}

TEST_P(QpackDecoderTest, EmptyNameAndValue) {
  EXPECT_CALL(handler_,
              OnHeaderDecoded(QuicStringPiece(""), QuicStringPiece("")));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode("2000"));
}

TEST_P(QpackDecoderTest, Simple) {
  EXPECT_CALL(handler_,
              OnHeaderDecoded(QuicStringPiece("foo"), QuicStringPiece("bar")));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode("23666f6f03626172"));
}

TEST_P(QpackDecoderTest, Multiple) {
  EXPECT_CALL(handler_,
              OnHeaderDecoded(QuicStringPiece("foo"), QuicStringPiece("bar")));
  QuicString str(127, 'a');
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("foobaar"),
                                        QuicStringPiece(str)));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode(
      "23666f6f03626172"    // foo: bar
      "2700666f6f62616172"  // 7 octet long header name, the smallest number
                            // that does not fit on a 3-bit prefix.
      "7f0061616161616161"  // 127 octet long header value, the smallest number
      "616161616161616161"  // that does not fit on a 7-bit prefix.
      "6161616161616161616161616161616161616161616161616161616161616161616161"
      "6161616161616161616161616161616161616161616161616161616161616161616161"
      "6161616161616161616161616161616161616161616161616161616161616161616161"
      "616161616161"));
}

TEST_P(QpackDecoderTest, NameLenTooLarge) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(
                            QuicStringPiece("Encoded integer too large.")));

  Decode(QuicTextUtils::HexDecode("27ffffffffffffffffffff"));
}

TEST_P(QpackDecoderTest, ValueLenTooLarge) {
  EXPECT_CALL(handler_,
              OnDecodingErrorDetected(QuicStringPiece("ValueLen too large.")));

  Decode(QuicTextUtils::HexDecode("23666f6f7fffffffffffffffffffff"));
}

TEST_P(QpackDecoderTest, IncompleteHeaderBlock) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(
                            QuicStringPiece("Incomplete header block.")));

  Decode(QuicTextUtils::HexDecode("2366"));
}

TEST_P(QpackDecoderTest, HuffmanSimple) {
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("custom-key"),
                                        QuicStringPiece("custom-value")));
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode(
      QuicStringPiece("2f0125a849e95ba97d7f8925a849e95bb8e8b4bf")));
}

TEST_P(QpackDecoderTest, AlternatingHuffmanNonHuffman) {
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("custom-key"),
                                        QuicStringPiece("custom-value")))
      .Times(4);
  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode(
      "2f0125a849e95ba97d7f"        // Huffman-encoded name.
      "8925a849e95bb8e8b4bf"        // Huffman-encoded value.
      "2703637573746f6d2d6b6579"    // Non-Huffman encoded name.
      "0c637573746f6d2d76616c7565"  // Non-Huffman encoded value.
      "2f0125a849e95ba97d7f"        // Huffman-encoded name.
      "0c637573746f6d2d76616c7565"  // Non-Huffman encoded value.
      "2703637573746f6d2d6b6579"    // Non-Huffman encoded name.
      "8925a849e95bb8e8b4bf"        // Huffman-encoded value.
      ));
}

TEST_P(QpackDecoderTest, HuffmanNameDoesNotHaveEOSPrefix) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(
                            QuicStringPiece("Error in Huffman-encoded name.")));

  // 'y' ends in 0b0 on the most significant bit of the last byte.
  // The remaining 7 bits must be a prefix of EOS, which is all 1s.
  Decode(QuicTextUtils::HexDecode("2f0125a849e95ba97d7e8925a849e95bb8e8b4bf"));
}

TEST_P(QpackDecoderTest, HuffmanValueDoesNotHaveEOSPrefix) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(QuicStringPiece(
                            "Error in Huffman-encoded value.")));

  // 'e' ends in 0b101, taking up the 3 most significant bits of the last byte.
  // The remaining 5 bits must be a prefix of EOS, which is all 1s.
  Decode(QuicTextUtils::HexDecode("2f0125a849e95ba97d7f8925a849e95bb8e8b4be"));
}

TEST_P(QpackDecoderTest, HuffmanNameEOSPrefixTooLong) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(
                            QuicStringPiece("Error in Huffman-encoded name.")));

  // The trailing EOS prefix must be at most 7 bits long.  Appending one octet
  // with value 0xff is invalid, even though 0b111111111111111 (15 bits) is a
  // prefix of EOS.
  Decode(
      QuicTextUtils::HexDecode("2f0225a849e95ba97d7fff8925a849e95bb8e8b4bf"));
}

TEST_P(QpackDecoderTest, HuffmanValueEOSPrefixTooLong) {
  EXPECT_CALL(handler_, OnDecodingErrorDetected(QuicStringPiece(
                            "Error in Huffman-encoded value.")));

  // The trailing EOS prefix must be at most 7 bits long.  Appending one octet
  // with value 0xff is invalid, even though 0b1111111111111 (13 bits) is a
  // prefix of EOS.
  Decode(
      QuicTextUtils::HexDecode("2f0125a849e95ba97d7f8a25a849e95bb8e8b4bfff"));
}

TEST_P(QpackDecoderTest, StaticTable) {
  // A header name that has multiple entries with different values.
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece(":method"),
                                        QuicStringPiece("GET")));
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece(":method"),
                                        QuicStringPiece("POST")));
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece(":method"),
                                        QuicStringPiece("TRACE")));

  // A header name that has a single entry with non-empty value.
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("accept-encoding"),
                                        QuicStringPiece("gzip, deflate, br")));
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("accept-encoding"),
                                        QuicStringPiece("compress")));
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("accept-encoding"),
                                        QuicStringPiece("")));

  // A header name that has a single entry with empty value.
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("location"),
                                        QuicStringPiece("")));
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("location"),
                                        QuicStringPiece("foo")));

  EXPECT_CALL(handler_, OnDecodingCompleted());

  Decode(QuicTextUtils::HexDecode(
      "d1dfccd45f108621e9aec2a11f5c8294e75f000554524143455f1000"));
}

TEST_P(QpackDecoderTest, TooHighStaticTableIndex) {
  // This is the last entry in the static table with index 98.
  EXPECT_CALL(handler_, OnHeaderDecoded(QuicStringPiece("x-frame-options"),
                                        QuicStringPiece("sameorigin")));

  // Addressing entry 99 should trigger an error.
  EXPECT_CALL(handler_, OnDecodingErrorDetected(
                            QuicStringPiece("Invalid static table index.")));

  Decode(QuicTextUtils::HexDecode("ff23ff24"));
}
}  // namespace
}  // namespace test
}  // namespace quic
