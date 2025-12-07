// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"

#include <string>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/encode/SkPngRustEncoder.h"

namespace blink {

TEST(ClipboardUtilitiesTest, URLToImageMarkupNonASCII) {
  // U+00E7 "Latin Small Letter C with Cedilla" is outside ASCII.
  // It has the UTF-8 encoding 0xC3 0xA7, but Blink interprets 8-bit string
  // literals as Latin-1 in most cases.
  String markup_with_non_ascii =
      URLToImageMarkup(KURL(NullURL(),
                            "http://test.example/fran\xe7"
                            "ais.png"),
                       "Fran\xe7"
                       "ais");
  EXPECT_EQ(
      "<img src=\"http://test.example/fran%C3%A7ais.png\" alt=\"Fran\xe7"
      "ais\"/>",
      markup_with_non_ascii);
  EXPECT_EQ(
      "<img src=\"http://test.example/fran%C3%A7ais.png\" alt=\"Fran\xc3\xa7"
      "ais\"/>",
      markup_with_non_ascii.Utf8());
}

TEST(ClipboardUtilitiesTest, URLToImageMarkupEmbeddedNull) {
  // Null characters, though strange, should also work.
  const char kURLWithNull[] = "http://test.example/\0.png";
  const char kTitleWithNull[] = "\0";
  const char kExpectedOutputWithNull[] =
      "<img src=\"http://test.example/%00.png\" alt=\"\0\"/>";
  EXPECT_EQ(String(base::span_from_cstring(kExpectedOutputWithNull)),
            URLToImageMarkup(
                KURL(NullURL(), String(base::span_from_cstring(kURLWithNull))),
                String(base::span_from_cstring(kTitleWithNull))));
}

TEST(ClipboardUtilitiesTest, PNGToImageMarkupEmpty) {
  EXPECT_TRUE(PNGToImageMarkup({}).IsNull());
}

TEST(ClipboardUtilitiesTest, PNGToImageMarkup) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(10, 5));
  SkPixmap pixmap;
  bitmap.peekPixels(&pixmap);

  // Set encoding options to favor speed over size.
  Vector<uint8_t> png_data;
  EXPECT_TRUE(ImageEncoder::Encode(&png_data, pixmap,
                                   SkPngRustEncoder::CompressionLevel::kLow));

  std::string markup = PNGToImageMarkup(png_data).Utf8();

  // The first 16 of a PNG file are always the same, so the
  // `StartsWith`/`EndsWith`-based assertions below are expected to succeed
  // regardless of the exact encoding settings or the encoding library used.
  EXPECT_THAT(
      markup,
      testing::StartsWith(
          R"HTML(<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhE)HTML"));
  EXPECT_THAT(markup, testing::EndsWith(R"HTML(" alt=""/>)HTML"));
}

}  // namespace blink
