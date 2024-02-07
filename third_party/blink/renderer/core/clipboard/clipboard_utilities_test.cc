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

TEST(ClipboardUtilitiesTest, AddMetaCharsetTagToHtmlOnMac) {
  const String html_markup = "<p>Test</p>";
  const String expected_html_markup = "<meta charset=\"utf-8\"><p>Test</p>";
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(AddMetaCharsetTagToHtmlOnMac(html_markup), expected_html_markup);
#else
  EXPECT_EQ(AddMetaCharsetTagToHtmlOnMac(html_markup), html_markup);
#endif
}

TEST(ClipboardUtilitiesTest, RemoveMetaTagAndCalcFragmentOffsetsFromHtmlOnMac) {
#if BUILDFLAG(IS_MAC)
  const String expected_html_markup = "<p>Test</p>";
  const String html_markup = "<meta charset=\"utf-8\"><p>Test</p>";
  unsigned fragment_start = 0;
  unsigned fragment_end = html_markup.length();
  const String actual_value = RemoveMetaTagAndCalcFragmentOffsetsFromHtmlOnMac(
      html_markup, fragment_start, fragment_end);
  EXPECT_EQ(actual_value, expected_html_markup);
  EXPECT_EQ(fragment_start, 0u);
  // Meta tag is not part of the copied fragment.
  EXPECT_EQ(fragment_end, expected_html_markup.length());
#else
  const String html_markup = "<p>Test</p>";
  unsigned fragment_start = 0;
  unsigned fragment_end = html_markup.length();
  const String actual_value = RemoveMetaTagAndCalcFragmentOffsetsFromHtmlOnMac(
      html_markup, fragment_start, fragment_end);
  // On non-Mac platforms, the HTML markup is unchanged.
  EXPECT_EQ(actual_value, html_markup);
  EXPECT_EQ(fragment_start, 0u);
  EXPECT_EQ(fragment_end, html_markup.length());
#endif
}

}  // namespace blink
