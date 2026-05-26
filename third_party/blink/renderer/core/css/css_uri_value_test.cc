// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_uri_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_url_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

// Verify that CSSUrlData::IsPotentiallyDanglingMarkup() correctly reflects
// the flag from a KURL whose raw input contained both whitespace and '<'.
TEST(CSSURIValueTest, DanglingMarkupFlagPreservedInCSSUrlData) {
  // Construct a KURL from a string with a newline and '<', which triggers
  // the potentially_dangling_markup flag during URL parsing.
  KURL dangling_url("ht\ntps://example.com/exfil?<secret");
  ASSERT_TRUE(dangling_url.PotentiallyDanglingMarkup());

  CSSUrlData* url_data = MakeGarbageCollected<CSSUrlData>(
      AtomicString("exfil?<secret"), dangling_url, Referrer(),
      /*origin_clean=*/true, /*is_ad_related=*/false,
      /*modifiers=*/CSSUrlRequestModifiers());

  EXPECT_TRUE(url_data->IsPotentiallyDanglingMarkup());

  // Constructing a KURL from the canonicalized string loses the flag — this
  // is the underlying bug that the fix addresses.
  KURL reconstructed(url_data->ResolvedUrl());
  EXPECT_FALSE(reconstructed.PotentiallyDanglingMarkup());
}

// Verify that CSSUrlData without dangling markup reports false.
TEST(CSSURIValueTest, NoDanglingMarkupFlag) {
  KURL safe_url("https://example.com/font.woff");
  ASSERT_FALSE(safe_url.PotentiallyDanglingMarkup());

  CSSUrlData* url_data = MakeGarbageCollected<CSSUrlData>(
      AtomicString("font.woff"), safe_url, Referrer(),
      /*origin_clean=*/true, /*is_ad_related=*/false,
      /*modifiers=*/CSSUrlRequestModifiers());

  EXPECT_FALSE(url_data->IsPotentiallyDanglingMarkup());
}

TEST(CSSURIValueTest, ComputedCSSValue) {
  cssvalue::CSSURIValue* rel = MakeGarbageCollected<cssvalue::CSSURIValue>(
      *MakeGarbageCollected<CSSUrlData>(
          AtomicString("a"), KURL("http://foo.com/a"), Referrer(),
          /*origin_clean=*/true, /*is_ad_related=*/false,
          /*modifiers=*/CSSUrlRequestModifiers()));
  cssvalue::CSSURIValue* abs =
      rel->ComputedCSSValue(KURL("http://bar.com"), TextEncoding());
  EXPECT_EQ("url(\"http://bar.com/a\")", abs->CssText());
}

TEST(CSSURIValueTest, AlreadyComputedCSSValue) {
  cssvalue::CSSURIValue* rel = MakeGarbageCollected<cssvalue::CSSURIValue>(
      *MakeGarbageCollected<CSSUrlData>(
          AtomicString("http://baz.com/a"), KURL("http://baz.com/a"),
          Referrer(), /*origin_clean=*/true, /*is_ad_related=*/false,
          /*modifiers=*/CSSUrlRequestModifiers()));
  cssvalue::CSSURIValue* abs =
      rel->ComputedCSSValue(KURL("http://bar.com"), TextEncoding());
  EXPECT_EQ("url(\"http://baz.com/a\")", abs->CssText());
}

TEST(CSSURIValueTest, LocalComputedCSSValue) {
  cssvalue::CSSURIValue* rel = MakeGarbageCollected<cssvalue::CSSURIValue>(
      *MakeGarbageCollected<CSSUrlData>(
          AtomicString("#a"), KURL("http://baz.com/a"), Referrer(),
          /*origin_clean=*/true, /*is_ad_related=*/false,
          /*modifiers=*/CSSUrlRequestModifiers()));
  cssvalue::CSSURIValue* abs =
      rel->ComputedCSSValue(KURL("http://bar.com"), TextEncoding());
  EXPECT_EQ("url(\"#a\")", abs->CssText());
}

TEST(CSSURIValueTest, EmptyComputedCSSValue) {
  cssvalue::CSSURIValue* rel = MakeGarbageCollected<cssvalue::CSSURIValue>(
      *MakeGarbageCollected<CSSUrlData>(
          g_empty_atom, KURL(), Referrer(),
          /*origin_clean=*/true,
          /*is_ad_related=*/false,
          /*modifiers=*/CSSUrlRequestModifiers()));
  cssvalue::CSSURIValue* abs =
      rel->ComputedCSSValue(KURL("http://bar.com"), TextEncoding());
  EXPECT_EQ("url(\"\")", abs->CssText());
}

}  // namespace
}  // namespace blink
