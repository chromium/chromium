// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prologue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace signed_exchange_prologue {

TEST(SignedExchangePrologueTest, Parse2BytesEncodedLength) {
  constexpr struct {
    uint8_t bytes[2];
    size_t expected;
  } kTestCases[] = {
      {{0x00, 0x01}, 1u}, {{0xab, 0xcd}, 43981u},
  };

  int test_element_index = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "testing case " << test_element_index++);
    EXPECT_EQ(Parse2BytesEncodedLength(test_case.bytes), test_case.expected);
  }
}

TEST(SignedExchangePrologueTest, Parse3BytesEncodedLength) {
  constexpr struct {
    uint8_t bytes[3];
    size_t expected;
  } kTestCases[] = {
      {{0x00, 0x00, 0x01}, 1u}, {{0x01, 0xe2, 0x40}, 123456u},
  };

  int test_element_index = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "testing case " << test_element_index++);
    EXPECT_EQ(Parse3BytesEncodedLength(test_case.bytes), test_case.expected);
  }
}

TEST(SignedExchangePrologueTest, BeforeFallbackUrl_Success) {
  uint8_t bytes[] = {'s', 'x', 'g', '1', '-', 'b', '3', '\0', 0x12, 0x34};

  BeforeFallbackUrl before_fallback_url = BeforeFallbackUrl::Parse(
      base::make_span(bytes), nullptr /* devtools_proxy */);
  EXPECT_TRUE(before_fallback_url.is_valid());
  EXPECT_EQ(0x1234u, before_fallback_url.fallback_url_length());
}

TEST(SignedExchangePrologueTest, BeforeFallbackUrl_B2) {
  uint8_t bytes[] = {'s', 'x', 'g', '1', '-', 'b', '2', '\0', 0x12, 0x34};

  BeforeFallbackUrl before_fallback_url = BeforeFallbackUrl::Parse(
      base::make_span(bytes), nullptr /* devtools_proxy */);
  EXPECT_FALSE(before_fallback_url.is_valid());
  EXPECT_EQ(0x1234u, before_fallback_url.fallback_url_length());
}

TEST(SignedExchangePrologueTest, BeforeFallbackUrl_WrongMagic) {
  uint8_t bytes[] = {'s', 'x', 'g', '!', '-', 'b', '3', '\0', 0x12, 0x34};

  BeforeFallbackUrl before_fallback_url = BeforeFallbackUrl::Parse(
      base::make_span(bytes), nullptr /* devtools_proxy */);
  EXPECT_FALSE(before_fallback_url.is_valid());
  EXPECT_EQ(0x1234u, before_fallback_url.fallback_url_length());
}

TEST(SignedExchangePrologueTest, FallbackUrlAndAfter_Success) {
  uint8_t bytes[] = {'h', 't', 't',  'p',  's',  ':',  '/',  '/', 'e',
                     'x', 'a', 'm',  'p',  'l',  'e',  '.',  'c', 'o',
                     'm', '/', 0x00, 0x12, 0x34, 0x00, 0x23, 0x45};

  BeforeFallbackUrl before_fallback_url(true,
                                        sizeof("https://example.com/") - 1);
  EXPECT_TRUE(before_fallback_url.is_valid());

  FallbackUrlAndAfter fallback_url_and_after =
      FallbackUrlAndAfter::Parse(base::make_span(bytes), before_fallback_url,
                                 nullptr /* devtools_proxy */);

  EXPECT_TRUE(fallback_url_and_after.is_valid());
  EXPECT_EQ("https://example.com/",
            fallback_url_and_after.fallback_url().url.spec());
  EXPECT_EQ(0x1234u, fallback_url_and_after.signature_header_field_length());
  EXPECT_EQ(0x2345u, fallback_url_and_after.cbor_header_length());
}

TEST(SignedExchangePrologueTest, FallbackUrlAndAfter_NonHttpsUrl) {
  uint8_t bytes[] = {'h', 't',  't',  'p',  ':',  '/',  '/', 'e', 'x',
                     'a', 'm',  'p',  'l',  'e',  '.',  'c', 'o', 'm',
                     '/', 0x00, 0x12, 0x34, 0x00, 0x23, 0x45};

  BeforeFallbackUrl before_fallback_url(true,
                                        sizeof("http://example.com/") - 1);
  FallbackUrlAndAfter fallback_url_and_after =
      FallbackUrlAndAfter::Parse(base::make_span(bytes), before_fallback_url,
                                 nullptr /* devtools_proxy */);

  EXPECT_FALSE(fallback_url_and_after.is_valid());
  EXPECT_FALSE(fallback_url_and_after.fallback_url().url.is_valid());
}

TEST(SignedExchangePrologueTest, FallbackUrlAndAfter_UrlWithFragment) {
  uint8_t bytes[] = {'h', 't', 't', 'p', 's',  ':',  '/',  '/',  'e',  'x',
                     'a', 'm', 'p', 'l', 'e',  '.',  'c',  'o',  'm',  '/',
                     '#', 'f', 'o', 'o', 0x00, 0x12, 0x34, 0x00, 0x23, 0x45};

  BeforeFallbackUrl before_fallback_url(true,
                                        sizeof("https://example.com/#foo") - 1);
  FallbackUrlAndAfter fallback_url_and_after =
      FallbackUrlAndAfter::Parse(base::make_span(bytes), before_fallback_url,
                                 nullptr /* devtools_proxy */);

  EXPECT_FALSE(fallback_url_and_after.is_valid());
  EXPECT_FALSE(fallback_url_and_after.fallback_url().url.is_valid());
}

TEST(SignedExchangePrologueTest, FallbackUrlAndAfter_LongSignatureHeader) {
  uint8_t bytes[] = {'h', 't', 't',  'p',  's',  ':',  '/',  '/', 'e',
                     'x', 'a', 'm',  'p',  'l',  'e',  '.',  'c', 'o',
                     'm', '/', 0xff, 0x12, 0x34, 0x00, 0x23, 0x45};

  BeforeFallbackUrl before_fallback_url(true,
                                        sizeof("https://example.com/") - 1);
  FallbackUrlAndAfter fallback_url_and_after =
      FallbackUrlAndAfter::Parse(base::make_span(bytes), before_fallback_url,
                                 nullptr /* devtools_proxy */);

  EXPECT_FALSE(fallback_url_and_after.is_valid());
  EXPECT_EQ("https://example.com/",
            fallback_url_and_after.fallback_url().url.spec());
}

TEST(SignedExchangePrologueTest, FallbackUrlAndAfter_LongCBORHeader) {
  uint8_t bytes[] = {'h', 't', 't',  'p',  's',  ':',  '/',  '/', 'e',
                     'x', 'a', 'm',  'p',  'l',  'e',  '.',  'c', 'o',
                     'm', '/', 0x00, 0x12, 0x34, 0xff, 0x23, 0x45};

  BeforeFallbackUrl before_fallback_url(true,
                                        sizeof("https://example.com/") - 1);
  FallbackUrlAndAfter fallback_url_and_after =
      FallbackUrlAndAfter::Parse(base::make_span(bytes), before_fallback_url,
                                 nullptr /* devtools_proxy */);

  EXPECT_FALSE(fallback_url_and_after.is_valid());
  EXPECT_EQ("https://example.com/",
            fallback_url_and_after.fallback_url().url.spec());
}

}  // namespace signed_exchange_prologue
}  // namespace content
