// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/clipboard_utilities.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

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
  EXPECT_EQ(
      String(kExpectedOutputWithNull, sizeof(kExpectedOutputWithNull) - 1),
      URLToImageMarkup(
          KURL(NullURL(), String(kURLWithNull, sizeof(kURLWithNull) - 1)),
          String(kTitleWithNull, sizeof(kTitleWithNull) - 1)));
}

TEST(ClipboardUtilitiesTest, PNGToImageMarkupEmpty) {
  EXPECT_TRUE(PNGToImageMarkup(mojo_base::BigBuffer()).IsNull());
}

TEST(ClipboardUtilitiesTest, PNGToImageMarkup) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(10, 5));
  SkPixmap pixmap;
  bitmap.peekPixels(&pixmap);

  // Set encoding options to favor speed over size.
  SkPngEncoder::Options options;
  options.fZLibLevel = 1;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

  Vector<uint8_t> png_data;
  EXPECT_TRUE(ImageEncoder::Encode(&png_data, pixmap, options));

  mojo_base::BigBuffer png = base::as_bytes(base::make_span(png_data));
  EXPECT_EQ(
      R"HTML(<img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAFCAYAAAB8ZH1oAAAADElEQVQYGWNgGEYAAADNAAGVVebMAAAAAElFTkSuQmCC" alt=""/>)HTML",
      PNGToImageMarkup(png));
}

}  // namespace blink
