// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/http2/hpack/huffman/hpack_huffman_encoder.h"

#include "base/macros.h"
#include "base/stl_util.h"
#include "net/third_party/http2/platform/api/http2_string_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace http2 {
namespace {

TEST(HuffmanEncoderTest, SpecRequestExamples) {
  Http2String test_table[] = {
      Http2HexDecode("f1e3c2e5f23a6ba0ab90f4ff"),
      "www.example.com",
      Http2HexDecode("a8eb10649cbf"),
      "no-cache",
      Http2HexDecode("25a849e95ba97d7f"),
      "custom-key",
      Http2HexDecode("25a849e95bb8e8b4bf"),
      "custom-value",
  };
  for (size_t i = 0; i != base::size(test_table); i += 2) {
    const Http2String& huffman_encoded(test_table[i]);
    const Http2String& plain_string(test_table[i + 1]);
    EXPECT_EQ(ExactHuffmanSize(plain_string), huffman_encoded.size());
    EXPECT_EQ(BoundedHuffmanSize(plain_string), huffman_encoded.size());
    Http2String buffer;
    buffer.reserve();
    HuffmanEncode(plain_string, &buffer);
    EXPECT_EQ(buffer, huffman_encoded) << "Error encoding " << plain_string;
  }
}

TEST(HuffmanEncoderTest, SpecResponseExamples) {
  // clang-format off
  Http2String test_table[] = {
    Http2HexDecode("6402"),
    "302",
    Http2HexDecode("aec3771a4b"),
    "private",
    Http2HexDecode("d07abe941054d444a8200595040b8166"
            "e082a62d1bff"),
    "Mon, 21 Oct 2013 20:13:21 GMT",
    Http2HexDecode("9d29ad171863c78f0b97c8e9ae82ae43"
            "d3"),
    "https://www.example.com",
    Http2HexDecode("94e7821dd7f2e6c7b335dfdfcd5b3960"
            "d5af27087f3672c1ab270fb5291f9587"
            "316065c003ed4ee5b1063d5007"),
    "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
  };
  // clang-format on
  for (size_t i = 0; i != base::size(test_table); i += 2) {
    const Http2String& huffman_encoded(test_table[i]);
    const Http2String& plain_string(test_table[i + 1]);
    EXPECT_EQ(ExactHuffmanSize(plain_string), huffman_encoded.size());
    EXPECT_EQ(BoundedHuffmanSize(plain_string), huffman_encoded.size());
    Http2String buffer;
    buffer.reserve(huffman_encoded.size());
    const size_t capacity = buffer.capacity();
    HuffmanEncode(plain_string, &buffer);
    EXPECT_EQ(buffer, huffman_encoded) << "Error encoding " << plain_string;
    EXPECT_EQ(capacity, buffer.capacity());
  }
}

TEST(HuffmanEncoderTest, EncodedSizeAgreesWithEncodeString) {
  Http2String test_table[] = {
      "",
      "Mon, 21 Oct 2013 20:13:21 GMT",
      "https://www.example.com",
      "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1",
      Http2String(1, '\0'),
      Http2String("foo\0bar", 7),
      Http2String(256, '\0'),
  };
  // Modify last |test_table| entry to cover all codes.
  for (size_t i = 0; i != 256; ++i) {
    test_table[base::size(test_table) - 1][i] = static_cast<char>(i);
  }

  for (size_t i = 0; i != base::size(test_table); ++i) {
    const Http2String& plain_string = test_table[i];
    Http2String huffman_encoded;
    HuffmanEncode(plain_string, &huffman_encoded);
    EXPECT_EQ(huffman_encoded.size(), ExactHuffmanSize(plain_string));
    EXPECT_LE(BoundedHuffmanSize(plain_string), plain_string.size());
    EXPECT_LE(BoundedHuffmanSize(plain_string), ExactHuffmanSize(plain_string));
  }
}

}  // namespace
}  // namespace http2
