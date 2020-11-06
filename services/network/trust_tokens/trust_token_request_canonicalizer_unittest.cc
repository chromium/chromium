// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"

#include <memory>

#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

// Adopt the Trust Tokens fixture to create URLRequests without boilerplate
using TrustTokenRequestCanonicalizerTest = TrustTokenRequestHelperTest;

// Check that an empty request with an empty public key (and no headers to sign)
// serializes correctly. Expected CBOR maps:
//
// SignRequestData::kHeadersOnly:
//   { "public_key": b"key" }
//
// SignRequestData::kInclude:
//   { "destination": "", "public_key": b"key" }
TEST_F(TrustTokenRequestCanonicalizerTest, Empty) {
  TrustTokenRequestCanonicalizer canonicalizer;

  cbor::Value::MapValue expected_cbor;
  expected_cbor[cbor::Value(
      TrustTokenRequestSigningHelper::kCanonicalizedRequestDataPublicKeyKey)] =
      cbor::Value("key", cbor::Value::Type::BYTE_STRING);

  std::unique_ptr<net::URLRequest> request = MakeURLRequest("");
  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kHeadersOnly),
      cbor::Writer::Write(cbor::Value(expected_cbor)));

  expected_cbor[cbor::Value(TrustTokenRequestSigningHelper::
                                kCanonicalizedRequestDataDestinationKey)] =
      cbor::Value("");
  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kInclude),
      cbor::Writer::Write(cbor::Value(expected_cbor)));
}

// Canonicalize a request with a nonempty public key and a nonempty URL.
//
// SignRequestData::kHeadersOnly:
//   { "public_key": b"key" }
//
// SignRequestData::kInclude:
//   { "destination": "issuer.com", "public_key": b"key" }
TEST_F(TrustTokenRequestCanonicalizerTest, Simple) {
  TrustTokenRequestCanonicalizer canonicalizer;

  cbor::Value::MapValue expected_cbor;
  expected_cbor[cbor::Value(
      TrustTokenRequestSigningHelper::kCanonicalizedRequestDataPublicKeyKey)] =
      cbor::Value("key", cbor::Value::Type::BYTE_STRING);

  std::unique_ptr<net::URLRequest> request =
      MakeURLRequest("https://sub.issuer.com/path?query");
  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kHeadersOnly),
      cbor::Writer::Write(cbor::Value(expected_cbor)));

  expected_cbor[cbor::Value(TrustTokenRequestSigningHelper::
                                kCanonicalizedRequestDataDestinationKey)] =
      cbor::Value("issuer.com");
  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kInclude),
      cbor::Writer::Write(cbor::Value(expected_cbor)));
}

// Canonicalize a request with a nonempty public key, some signed headers, and a
// nonempty URL.
//
// Expected CBOR maps:
//
// SignRequestData::kHeadersOnly:
//   { "public_key": b"key", "first_header": "first_header_value",
//     "second_header": "second_header_value" }
//
// SignRequestData::kInclude:
//   { "destination": "issuer.com", "public_key": b"key",
//     "first_header": "first_header_value", "second_header":
//     "second_header_value" }
TEST_F(TrustTokenRequestCanonicalizerTest, WithSignedHeaders) {
  TrustTokenRequestCanonicalizer canonicalizer;

  cbor::Value::MapValue expected_cbor;
  expected_cbor[cbor::Value(
      TrustTokenRequestSigningHelper::kCanonicalizedRequestDataPublicKeyKey)] =
      cbor::Value("key", cbor::Value::Type::BYTE_STRING);

  std::unique_ptr<net::URLRequest> request =
      MakeURLRequest("https://sub.issuer.com/path?query");

  // Capitalization should be normalized.
  request->SetExtraRequestHeaderByName("First_HeadER", "first_header_value",
                                       /*overwrite=*/true);

  request->SetExtraRequestHeaderByName("second_header", "second_header_value",
                                       /*overwrite=*/true);
  request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders,
                                       "  first_header ,  second_header ",
                                       /*overwrite=*/true);

  expected_cbor[cbor::Value("first_header")] =
      cbor::Value("first_header_value");
  expected_cbor[cbor::Value("second_header")] =
      cbor::Value("second_header_value");

  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kHeadersOnly),
      cbor::Writer::Write(cbor::Value(expected_cbor)));

  expected_cbor[cbor::Value(TrustTokenRequestSigningHelper::
                                kCanonicalizedRequestDataDestinationKey)] =
      cbor::Value("issuer.com");
  EXPECT_EQ(
      canonicalizer.Canonicalize(
          request->url(), request->extra_request_headers(),
          /*public_key=*/"key", mojom::TrustTokenSignRequestData::kInclude),
      cbor::Writer::Write(cbor::Value(expected_cbor)));
}

// Canonicalizing a request with a malformed Signed-Headers header should fail.
TEST_F(TrustTokenRequestCanonicalizerTest, RejectsMalformedSignedHeaders) {
  TrustTokenRequestCanonicalizer canonicalizer;

  std::unique_ptr<net::URLRequest> request =
      MakeURLRequest("https://issuer.com/");

  // Set the Signed-Headers header to something that is *not* the serialization
  // of a Structured Headers token. (Tokens can't start with quotes.)
  request->SetExtraRequestHeaderByName(kTrustTokensRequestHeaderSignedHeaders,
                                       "\"", /*overwrite=*/true);

  EXPECT_FALSE(canonicalizer.Canonicalize(
      request->url(), request->extra_request_headers(),
      /*public_key=*/"key", mojom::TrustTokenSignRequestData::kHeadersOnly));
}

// Canonicalizing a request with an empty key should fail.
TEST_F(TrustTokenRequestCanonicalizerTest, RejectsEmptyKey) {
  TrustTokenRequestCanonicalizer canonicalizer;

  std::unique_ptr<net::URLRequest> request =
      MakeURLRequest("https://issuer.com/");

  EXPECT_FALSE(canonicalizer.Canonicalize(
      request->url(), request->extra_request_headers(),
      /*public_key=*/"", mojom::TrustTokenSignRequestData::kHeadersOnly));
}
}  // namespace network
