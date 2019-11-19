// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_prologue.h"
#include "content/public/common/content_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kSignatureString[] =
    "sig1;"
    " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
    "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
    " integrity=\"mi-draft2\";"
    " validity-url=\"https://test.example.org/resource.validity.1511128380\";"
    " cert-url=\"https://example.com/oldcerts\";"
    " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
    " date=1511128380; expires=1511733180";

cbor::Value CBORByteString(const char* str) {
  return cbor::Value(str, cbor::Value::Type::BYTE_STRING);
}

base::Optional<SignedExchangeEnvelope> GenerateHeaderAndParse(
    SignedExchangeVersion version,
    base::StringPiece fallback_url,
    base::StringPiece signature,
    const std::map<const char*, const char*>& response_map) {
  cbor::Value::MapValue response_cbor_map;
  for (auto& pair : response_map)
    response_cbor_map[CBORByteString(pair.first)] = CBORByteString(pair.second);

  DCHECK_EQ(version, SignedExchangeVersion::kB3);
  auto serialized =
      cbor::Writer::Write(cbor::Value(std::move(response_cbor_map)));
  return SignedExchangeEnvelope::Parse(
      version, signed_exchange_utils::URLWithRawString(fallback_url), signature,
      base::make_span(serialized->data(), serialized->size()),
      nullptr /* devtools_proxy */);
}

}  // namespace

class SignedExchangeEnvelopeTest
    : public ::testing::TestWithParam<SignedExchangeVersion> {};

TEST_P(SignedExchangeEnvelopeTest, ParseGoldenFile) {
  base::FilePath test_sxg_path;
  base::PathService::Get(content::DIR_TEST_DATA, &test_sxg_path);
  test_sxg_path =
      test_sxg_path.AppendASCII("sxg").AppendASCII("test.example.org_test.sxg");

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(test_sxg_path, &contents));
  auto* contents_bytes = reinterpret_cast<const uint8_t*>(contents.data());

  ASSERT_GT(contents.size(),
            signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes);
  signed_exchange_prologue::BeforeFallbackUrl prologue_a =
      signed_exchange_prologue::BeforeFallbackUrl::Parse(
          base::make_span(
              contents_bytes,
              signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes),
          nullptr /* devtools_proxy */);
  ASSERT_GT(contents.size(),
            signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes +
                prologue_a.ComputeFallbackUrlAndAfterLength());
  signed_exchange_prologue::FallbackUrlAndAfter prologue_b =
      signed_exchange_prologue::FallbackUrlAndAfter::Parse(
          base::make_span(contents_bytes +
                              signed_exchange_prologue::BeforeFallbackUrl::
                                  kEncodedSizeInBytes,
                          prologue_a.ComputeFallbackUrlAndAfterLength()),
          prologue_a, nullptr /* devtools_proxy */);

  size_t signature_header_field_offset =
      signed_exchange_prologue::BeforeFallbackUrl::kEncodedSizeInBytes +
      prologue_a.ComputeFallbackUrlAndAfterLength();
  base::StringPiece signature_header_field(
      contents.data() + signature_header_field_offset,
      prologue_b.signature_header_field_length());
  const auto cbor_bytes =
      base::make_span(contents_bytes + signature_header_field_offset +
                          prologue_b.signature_header_field_length(),
                      prologue_b.cbor_header_length());
  const base::Optional<SignedExchangeEnvelope> envelope =
      SignedExchangeEnvelope::Parse(
          SignedExchangeVersion::kB3, prologue_b.fallback_url(),
          signature_header_field, cbor_bytes, nullptr /* devtools_proxy */);
  ASSERT_TRUE(envelope.has_value());
  EXPECT_EQ(envelope->request_url().url,
            GURL("https://test.example.org/test/"));
  EXPECT_EQ(envelope->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(envelope->response_headers().size(), 3u);
  EXPECT_EQ(envelope->response_headers().find("content-encoding")->second,
            "mi-sha256-03");
}

TEST_P(SignedExchangeEnvelopeTest, ValidHeader) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {{kStatusKey, "200"}, {"content-type", "text/html"}, {"digest", "foo"}});
  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(header->request_url().url, GURL("https://test.example.org/test/"));
  EXPECT_EQ(header->response_code(), static_cast<net::HttpStatusCode>(200u));
  EXPECT_EQ(header->response_headers().size(), 2u);
}

TEST_P(SignedExchangeEnvelopeTest, InformationalResponseCode) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "100"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, RelativeURL) {
  auto header = GenerateHeaderAndParse(GetParam(), "test/", kSignatureString,
                                       {
                                           {kStatusKey, "200"},
                                       });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, HttpURLShouldFail) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "http://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, RedirectStatusShouldFail) {
  auto header =
      GenerateHeaderAndParse(GetParam(), "https://test.example.org/test/",
                             kSignatureString, {{kStatusKey, "302"}});
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, Status300ShouldFail) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {{kStatusKey, "300"}});  // 300 is not a redirect status.
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, StatefulResponseHeader) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"set-cookie", "foo=bar"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, UppercaseResponseMap) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {{kStatusKey, "200"}, {"Content-Length", "123"}});
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, InvalidValidityURLHeader) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test2.example.org/test/", kSignatureString,
      {{kStatusKey, "200"}, {"content-type", "text/html"}});
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, InnerResponseIsSXG) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {{kStatusKey, "200"},
       {"content-type", "application/signed-exchange;v=b3"}});
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, CacheControlNoStore) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"cache-control", "no-store"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, CacheControlSecondValueIsNoStore) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"cache-control", "max-age=300, no-store"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, CacheControlPrivateWithValue) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"cache-control", "private=foo"},
      });
  ASSERT_FALSE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, CacheControlNoStoreInQuotedString) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"cache-control", "foo=\"300, no-store\""},
          {"digest", "foo"},
      });
  ASSERT_TRUE(header.has_value());
}

TEST_P(SignedExchangeEnvelopeTest, CacheControlParseError) {
  auto header = GenerateHeaderAndParse(
      GetParam(), "https://test.example.org/test/", kSignatureString,
      {
          {kStatusKey, "200"},
          {"cache-control", "max-age=\"abc"},
      });
  ASSERT_FALSE(header.has_value());
}

INSTANTIATE_TEST_SUITE_P(SignedExchangeEnvelopeTests,
                         SignedExchangeEnvelopeTest,
                         ::testing::Values(SignedExchangeVersion::kB3));

}  // namespace content
