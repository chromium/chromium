// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder_builder.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

static const WTF::TextEncoding DefaultEncodingForUrlAndContentType(
    const char* url,
    const char* content_type) {
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(0, 0));
  Document& document = page_holder->GetDocument();
  document.SetURL(KURL(NullURL(), url));
  return BuildTextResourceDecoder(document.GetFrame(), document.Url(),
                                  AtomicString(content_type), g_null_atom)
      ->Encoding();
}

static const WTF::TextEncoding DefaultEncodingForURL(const char* url) {
  return DefaultEncodingForUrlAndContentType(url, "text/html");
}

TEST(TextResourceDecoderBuilderTest, defaultEncodingForJsonIsUTF8) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(WTF::TextEncoding("UTF-8"),
            DefaultEncodingForUrlAndContentType(
                "https://udarenieru.ru/1.2/dealers/", "application/json"));
}

TEST(TextResourceDecoderBuilderTest, defaultEncodingComesFromTopLevelDomain) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(WTF::TextEncoding("Shift_JIS"),
            DefaultEncodingForURL("http://tsubotaa.la.coocan.jp"));
  EXPECT_EQ(WTF::TextEncoding("windows-1251"),
            DefaultEncodingForURL("http://udarenieru.ru/index.php"));
}

TEST(TextResourceDecoderBuilderTest,
     NoCountryDomainURLDefaultsToLatin1Encoding) {
  test::TaskEnvironment task_environment;
  // Latin1 encoding is set in |TextResourceDecoder::defaultEncoding()|.
  EXPECT_EQ(WTF::Latin1Encoding(),
            DefaultEncodingForURL("http://arstechnica.com/about-us"));
}

}  // namespace blink
