// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/sri_message_signature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

// Exciting test constants, leaning on test data from the RFC.
//
// Base64 encoded Ed25519 Test Keys, pulled from the RFC at
// https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4
const char* kPublicKey = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";

// Another base64 encoded Ed25519 key, randomly generated:
//
// {
//   "crv": "Ed25519",
//   "d": "MTodZiTA9CBsuIvSfO679TThkG3b7ce6R3sq_CdyVp4",
//   "ext": true,
//   "kty": "OKP",
//   "x": "xDnP380zcL4rJ76rXYjeHlfMyPZEOqpJYjsjEppbuXE"
// }
const char* kPublicKey2 = "xDnP380zcL4rJ76rXYjeHlfMyPZEOqpJYjsjEppbuXE=";

// The following constants are extracted from this known-good response that
// matches the constraints described in
// https://wicg.github.io/signature-based-sri/#verification-requirements-for-sri
//
// ```
// HTTP/1.1 200 OK
// Date: Tue, 20 Apr 2021 02:07:56 GMT
// Content-Type: application/json
// Unencoded-Digest: sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:
// Content-Length: 18
// Signature-Input: signature=("unencoded-digest";sf); \
//                  keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs="; \
//                  tag="ed25519-integrity"
// Signature: signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj \
//                       5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==:
//
// {"hello": "world"}
// ```
const char* kSignature =
    "SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/"
    "JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==";

const char* kValidDigestHeader =
    "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
const char* kValidDigestHeader512 =
    "sha-512=:WZDPaVn/7XgHaAy8pmojAkGWoRx2UFChF41A2svX+TaPm+AbwAgBWnrIiYllu7BNN"
    "yealdVLvRwEmTHWXvJwew==:";

// A basic signature header set with no expiration.
const char* kValidSignatureInputHeader =
    "signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
    "89iXES9+vFgrIy29clF9CC/"
    "oPPsw3c5D0bs=\";tag=\"ed25519-integrity\"";
const char* kUnusedSignatureInputHeader =
    "unused-signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
    "89iXES9+vFgrIy29"
    "clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\"";
const char* kValidSignatureHeader =
    "signature=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/"
    "JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:";
const char* kUnusedSignatureHeader =
    "unused-input=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/"
    "JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:";

// The following signature was generated using test-key-ed25519 from RFC 9421
// (https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4),
// the same key used for generating the constants above.
//
// A valid signature header set with expiration in the future (2142-12-30).
const char* kValidExpiringSignatureInputHeader =
    "signature=(\"unencoded-digest\";sf);expires=5459212800;"
    "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/"
    "oPPsw3c5D0bs=\";tag=\"ed25519-integrity\"";
const char* kValidExpiringSignatureHeader =
    "signature=:"
    "y1gsTrDU7XUJzdk6o3IIVjc2WqEC3duoEmD9KczAp7OUDU8CwjD1PfYhIVeTOieGzeuCjQR+"
    "9JwWJV2BF41uBA==:";
const int64_t kValidExpiringSignatureExpiresAt = 5459212800;

constexpr std::string_view kAcceptSignature = "accept-signature";

const GURL kExampleURL = GURL("https://example.test/");

std::unique_ptr<net::URLRequest> CreateRequest(
    const net::URLRequestContext& context,
    const GURL& url) {
  std::unique_ptr<net::URLRequest> request =
      context.CreateRequest(url, net::DEFAULT_PRIORITY, /*delegate=*/nullptr,
                            TRAFFIC_ANNOTATION_FOR_TESTS);
  return request;
}

std::unique_ptr<net::URLRequest> CreateRequest(
    const net::URLRequestContext& context,
    const std::string_view url_string) {
  return CreateRequest(context, GURL(url_string));
}

}  // namespace

class SRIMessageSignatureParserTest : public testing::Test {
 protected:
  SRIMessageSignatureParserTest() = default;

  scoped_refptr<net::HttpResponseHeaders> GetHeaders(const char* signature,
                                                     const char* input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (signature) {
      builder.AddHeader("Signature", signature);
    }
    if (input) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }

  const GURL& url() { return kExampleURL; }

  void ValidateBasicTestHeader(const mojom::SRIMessageSignaturePtr& sig) {
    EXPECT_EQ("signature", sig->label);
    EXPECT_EQ(std::nullopt, sig->created);
    EXPECT_EQ(std::nullopt, sig->expires);
    EXPECT_EQ(base::Base64Decode(kPublicKey), sig->keyid);
    EXPECT_EQ(std::nullopt, sig->nonce);
    EXPECT_EQ("ed25519-integrity", sig->tag);
    EXPECT_EQ(kSignature, base::Base64Encode(sig->signature));

    ASSERT_EQ(1u, sig->components.size());
    EXPECT_EQ("unencoded-digest", sig->components[0]->name);
    ASSERT_EQ(1u, sig->components[0]->params.size());
    EXPECT_EQ(mojom::SRIMessageSignatureComponentParameter::Type::
                  kStrictStructuredFieldSerialization,
              sig->components[0]->params[0]->type);
  }
};

TEST_F(SRIMessageSignatureParserTest, NoHeaders) {
  auto headers = GetHeaders(/*signature=*/nullptr, /*input=*/nullptr);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  EXPECT_EQ(0u, result->issues.size());
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureHeader) {
  auto headers = GetHeaders(/*signature=*/nullptr, kValidSignatureInputHeader);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  ASSERT_EQ(1u, result->issues.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kMissingSignatureHeader,
            result->issues[0]->error);
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureInputHeader) {
  auto headers = GetHeaders(kValidSignatureHeader, /*input=*/nullptr);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result->signatures.size());
  ASSERT_EQ(1u, result->issues.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kMissingSignatureInputHeader,
            result->issues[0]->error);
}

TEST_F(SRIMessageSignatureParserTest, ValidHeaders) {
  auto headers = GetHeaders(kValidSignatureHeader, kValidSignatureInputHeader);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);

  EXPECT_EQ(1u, result->signatures.size());
  EXPECT_EQ(0u, result->issues.size());
  ValidateBasicTestHeader(result->signatures[0]);
}

// TODO(crbug.com/419149647): Drop support for `sri` once tests are updated and
// OT participants have adopted the new `tag`.
TEST_F(SRIMessageSignatureParserTest, SriTagSupport) {
  // Same as kValidSignatureInputHeader and kValidSignatureHeader, but using a
  // `tag` of "sri" instead of "ed25519-input".
  const char* sri_input_header =
      "signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
      "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
  const char* sri_signature_header =
      "signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+"
      "ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ9p7C0hzW4o4Qpkm5aApd6WLLCw==:";
  auto headers = GetHeaders(sri_signature_header, sri_input_header);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);

  EXPECT_EQ(1u, result->signatures.size());
  EXPECT_EQ(0u, result->issues.size());
  // The remainder of the signature properties are validated in `ValidHeaders`
  // (and many other places); here we only care about the `tag`.
  EXPECT_EQ("sri", result->signatures[0]->tag);
}

TEST_F(SRIMessageSignatureParserTest, UnmatchedLabelsInAdditionToValidHeaders) {
  // kValidSignatureInputHeader defines inputs for the `signature` label. The
  // following header will define a signature for that label, as well as another
  // signature with an unused label.
  //
  // We're currently ignoring this mismatched signature, and therefore treating
  // the header as valid.
  std::string two_signatures =
      base::StrCat({kUnusedSignatureHeader, ",", kValidSignatureHeader});
  std::string two_inputs = base::StrCat(
      {kUnusedSignatureInputHeader, ",", kValidSignatureInputHeader});

  // Too many signatures:
  {
    auto headers =
        GetHeaders(two_signatures.c_str(), kValidSignatureInputHeader);
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    EXPECT_EQ(1u, result->issues.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel,
        result->issues[0]->error);
    ValidateBasicTestHeader(result->signatures[0]);
  }

  // Too many inputs:
  {
    auto headers = GetHeaders(kValidSignatureHeader, two_inputs.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    // TODO(crbug.com/381044049): We should probably have a parsing error here.
    EXPECT_EQ(0u, result->issues.size());
    ValidateBasicTestHeader(result->signatures[0]);
  }

  // Too many everythings!
  {
    auto headers = GetHeaders(two_signatures.c_str(), two_inputs.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result->signatures.size());
    EXPECT_EQ(1u, result->issues.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel,
        result->issues[0]->error);
    ValidateBasicTestHeader(result->signatures[0]);
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureHeader) {
  const char* cases[] = {
      // Non-dictionaries
      "",
      "1",
      "1.1",
      "\"string\"",
      "token",
      ":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
      "?0",
      "@12345",
      "%\"display\"",
      "A, list, of, tokens",
      "(inner list)",

      // Dictionaries with non-byte-sequence values.
      "signature=",
      "signature=1",
      "signature=1.1",
      "signature=\"string\"",
      "signature=token",
      "signature=?0",
      "signature=@12345",
      "signature=%\"display\"",
      "signature=(inner list of tokens)",

      // Dictionaries with byte-sequence values of the wrong length:
      "signature=:YQ==:",

      // Parameterized, but otherwise correct byte-sequence values:
      ("signature=:amDAmvl9bsfIcfA/bIJsBuBvInjJAaxxNIlLOzNI3FkrnG2k52UxXJprz89"
       "+2aOwEAz3w6KjjZuGkdrOUwxhBQ==:;param=1"),
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    auto headers = GetHeaders(test, kValidSignatureInputHeader);
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
    EXPECT_EQ(1u, result->issues.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputComponents) {
  struct {
    const char* value;
    mojom::SRIMessageSignatureError error;
  } cases[] = {
      // Non-dictionaries:
      {"", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"1", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"1.1", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"\"string\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"?0", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"A, list, of, tokens",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"(inner list)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"@12345", mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"%\"display\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Dictionaries with non-inner-list values:
      {"signature",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=1",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=1.1",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=\"string\"",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=token",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=?0",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      {"signature=:badbeef:",
       mojom::SRIMessageSignatureError::kSignatureInputHeaderValueNotInnerList},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=@12345",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=%\"display\"",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Dictionaries with inner-list values that contain non-strings:
      {"signature=()", mojom::SRIMessageSignatureError::
                           kSignatureInputHeaderValueMissingComponents},
      {"signature=(1)", mojom::SRIMessageSignatureError::
                            kSignatureInputHeaderInvalidComponentType},
      {"signature=(1.1)", mojom::SRIMessageSignatureError::
                              kSignatureInputHeaderInvalidComponentType},
      {"signature=(token)", mojom::SRIMessageSignatureError::
                                kSignatureInputHeaderInvalidComponentType},
      {"signature=(:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      {"signature=(?0)", mojom::SRIMessageSignatureError::
                             kSignatureInputHeaderInvalidComponentType},
      {"signature=(A list of tokens)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=(@12345)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(%\"display\")",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // Components that are valid per-spec, but aren't quite constrained enough
      // for SRI's initial implementation. We'll eventually treat these as valid
      // headers, but they're parse errors for now.
      {"signature=(\"invalid header names\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"@unknown-derived-components\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"Unencoded-Digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"UNENCODED-DIGEST\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentName},
      {"signature=(\"unencoded-digest\" \"and-something-else\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"something-else\" \"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},

      // Invalid component params:
      {"signature=(\"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(\"unencoded-digest\";sf=1)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=1.1)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=\"string\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=token)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=?0)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf=:badbeef:)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"unencoded-digest\";sf;not-sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      // We don't yet support dates or display strings, so these are invalid
      // headers, not invalid types.
      {"signature=(\"unencoded-digest\";sf=@12345)",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},
      {"signature=(\"unencoded-digest\";sf=%\"display\")",
       mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader},

      // One valid, one invalid component:
      {"signature=(\"unencoded-digest\";sf \"@path\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"@path\" \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"@status\";req \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"unencoded-digest\";sf \"@status\";req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"unencoded-digest\";sf \"@query-param\";req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"unencoded-digest\";sf \"@query-param\";name=\"a\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidDerivedComponentParameter},
      {"signature=(\"unencoded-digest\";sf token;req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},
      {"signature=(\"unencoded-digest\";sf \"header-with-sf\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(token;req \"unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidComponentType},

      // Valid component without valid `unencoded-digest`:
      {"signature=(\"unencoded-digest\" \"@path\";req)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"@path\";req \"unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
      {"signature=(\"@path\";req \"not-unencoded-digest\")",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderValueMissingComponents},
      {"signature=(\"@path\";req \"not-unencoded-digest\";sf)",
       mojom::SRIMessageSignatureError::
           kSignatureInputHeaderInvalidHeaderComponentParameter},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");

    // Tack valid parameters onto the test string so that we're actually
    // just testing the component parsing.
    std::string test_with_params = base::StrCat(
        {test.value, ";keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});
    auto headers = GetHeaders(kValidSignatureHeader, test_with_params.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
    ASSERT_GT(result->issues.size(), 0u);
    EXPECT_THAT(
        result->issues,
        testing::Contains(testing::Pointee(testing::Field(
            "error", &mojom::SRIMessageSignatureIssue::error, test.error))));
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputParameters) {
  const char* cases[] = {
      // Missing a required parameter:
      "keyid=\"[KEY]\"",
      "tag=\"ed25519-integrity\"",

      // Duplication (insofar as the invalid value comes last):
      "keyid=\"[KEY]\";keyid=\"not-[KEY]\";tag=\"ed25519-integrity\"",
      "keyid=\"[KEY]\";tag=\"ed25519-integrity\";tag=\"not-sri\"",

      // Alg is present:
      "alg=;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=1.1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=token;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=?0;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=@12345;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=%\"display\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=:badbeef:;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",

      // Invalid `created`:
      //
      // - Types:
      "created=;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=1.1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=\"string\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=token;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=?0;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=@12345;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=%\"display\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "created=:badbeef:;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      // - Values
      "created=-1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",

      // Invalid `expires`:
      //
      // - Types:
      "expires=;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=1.1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=\"string\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=token;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=?0;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=@12345;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=%\"display\";keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      "expires=:badbeef:;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",
      // - Values
      "expires=-1;keyid=\"[KEY]\";tag=\"ed25519-integrity\"",

      // Invalid `keyid`:
      //
      // - Types
      "keyid=;tag=\"ed25519-integrity\"",
      "keyid=1;tag=\"ed25519-integrity\"",
      "keyid=1.1;tag=\"ed25519-integrity\"",
      "keyid=token;tag=\"ed25519-integrity\"",
      "keyid=?0;tag=\"ed25519-integrity\"",
      "keyid=@12345;tag=\"ed25519-integrity\"",
      "keyid=%\"display\";tag=\"ed25519-integrity\"",
      "keyid=:badbeef:;tag=\"ed25519-integrity\"",
      // - Values
      "keyid=\"not a base64-encoded key\";tag=\"ed25519-integrity\"",

      // Invalid `nonce`:
      //
      // - Types
      "keyid=\"[KEY]\";nonce=;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=1;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=1.1;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=token;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=?0;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=@12345;tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=%\"display\";tag=\"not-sri\"",
      "keyid=\"[KEY]\";nonce=:badbeef:;tag=\"not-sri\"",

      // Invalid `tag`:
      //
      // - Types
      "keyid=\"[KEY]\";tag=",
      "keyid=\"[KEY]\";tag=1",
      "keyid=\"[KEY]\";tag=1.1",
      "keyid=\"[KEY]\";tag=token",
      "keyid=\"[KEY]\";tag=?0",
      "keyid=\"[KEY]\";tag=@12345",
      "keyid=\"[KEY]\";tag=%\"display\"",
      "keyid=\"[KEY]\";tag=:badbeef:",
      // - Values
      "keyid=\"[KEY]\";tag=\"not-sri\"",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);", test});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result->signatures.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, NonSRITag) {
  const char* non_sri_signature_input =
      "signature=(\"something-invalid-for-sri\");keyid=\"also-invalid\";tag="
      "\"not-sri\"";

  auto headers = GetHeaders(kValidSignatureHeader, non_sri_signature_input);
  mojom::SRIMessageSignaturesPtr result =
      ParseSRIMessageSignaturesFromHeaders(*headers);

  EXPECT_EQ(0u, result->signatures.size());
  EXPECT_EQ(0u, result->issues.size());
}

TEST_F(SRIMessageSignatureParserTest, ValidComponents) {
  struct {
    std::string_view components;
    std::vector<std::string_view> expected_names;
  } cases[] = {
      {"\"unencoded-digest\";sf", {"unencoded-digest"}},
      {"\"unencoded-digest\";sf \"@path\";req", {"unencoded-digest", "@path"}},
      {"\"@path\";req \"unencoded-digest\";sf", {"@path", "unencoded-digest"}}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Component value: `" << test.components << "`");

    // Tack valid parameters onto the test string so that we're actually
    // just testing the component parsing.
    std::string test_with_params =
        base::StrCat({"signature=(", test.components, ");keyid=\"", kPublicKey,
                      "\";tag=\"ed25519-integrity\""});
    auto headers = GetHeaders(kValidSignatureHeader, test_with_params.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->issues.size());
    ASSERT_EQ(test.expected_names.size(),
              result->signatures[0]->components.size());
    for (size_t i = 0; i < test.expected_names.size(); i++) {
      EXPECT_EQ(test.expected_names[i],
                result->signatures[0]->components[i]->name);
    }
  }
}

TEST_F(SRIMessageSignatureParserTest, Created) {
  const char* cases[] = {
      "0",
      "1",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Created value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);created=", test,
                      ";keyid=\"[KEY]\";tag=\"ed25519-integrity\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->issues.size());
    ASSERT_TRUE(result->signatures[0]->created.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result->signatures[0]->created.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, Expires) {
  const char* cases[] = {
      "0",
      "1",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Expires value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);expires=", test,
                      ";keyid=\"[KEY]\";tag=\"ed25519-integrity\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->issues.size());
    ASSERT_TRUE(result->signatures[0]->expires.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result->signatures[0]->expires.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, Nonce) {
  const char* cases[] = {
      "valid",
      "also valid",
      "999999999999999",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Nonce value: `" << test << "`");

    // Build the header.
    std::string processed_input =
        base::StrCat({"signature=(\"unencoded-digest\";sf);keyid=\"[KEY]\";",
                      "nonce=\"", test, "\";tag=\"ed25519-integrity\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    mojom::SRIMessageSignaturesPtr result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->issues.size());
    ASSERT_TRUE(result->signatures[0]->nonce.has_value());
    EXPECT_EQ(test, result->signatures[0]->nonce.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, ParameterSorting) {
  std::vector<const char*> params = {
      "created=12345", "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"", "nonce=\"n\"",
      "tag=\"ed25519-integrity\""};

  do {
    std::stringstream header;
    header << "signature=(\"unencoded-digest\";sf)";
    for (const char* param : params) {
      header << ';' << param;
    }
    SCOPED_TRACE(header.str());
    auto headers = GetHeaders(kValidSignatureHeader, header.str().c_str());
    auto result = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, result->signatures.size());
    EXPECT_EQ(0u, result->issues.size());
  } while (std::next_permutation(params.begin(), params.end()));
}

//
// "Signature Base" Creation Tests
//
class SRIMessageSignatureBaseTest : public testing::Test {
 protected:
  SRIMessageSignatureBaseTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()),
        request_(CreateRequest(*context_, kExampleURL)) {}

  const net::URLRequest& request() { return *request_; }

  scoped_refptr<net::HttpResponseHeaders> ValidHeadersPlusInput(
      const char* input) {
    return ValidHeadersPlusInputAndStatus(input, 200);
  }

  scoped_refptr<net::HttpResponseHeaders> ValidHeadersPlusInputAndStatus(
      const char* input,
      const int status_code) {
    std::string status_string = base::NumberToString(status_code);
    auto builder = net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1),
                                                     status_string);
    builder.AddHeader("Unencoded-Digest", kValidDigestHeader);
    builder.AddHeader("Signature", kValidSignatureHeader);
    if (input) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> request_;
};

TEST_F(SRIMessageSignatureBaseTest, NoSignaturesNoBase) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  mojom::SRIMessageSignaturePtr signature;

  std::optional<std::string> result =
      ConstructSignatureBase(signature, request(), *headers);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersValidBase) {
  auto headers = ValidHeadersPlusInput(kValidSignatureInputHeader);
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  std::optional<std::string> result =
      ConstructSignatureBase(parsed->signatures[0], request(), *headers);
  ASSERT_TRUE(result.has_value());
  std::string expected_base =
      base::StrCat({"\"unencoded-digest\";sf: ", kValidDigestHeader,
                    "\n\"@signature-params\": "
                    "(\"unencoded-digest\";sf);keyid=\"",
                    kPublicKey, "\";tag=\"ed25519-integrity\""});
  EXPECT_EQ(expected_base, result.value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersStrictlySerializedBase) {
  // Regardless of (valid) whitespace, the signature base is strictly
  // serialized.
  const char* cases[] = {
      // Base
      ("signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\""),
      // Leading space.
      (" signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\""),
      // Space before inner-list item.
      ("signature=( \"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\""),
      // Space after `;` in a param.
      ("signature=(\"unencoded-digest\"; sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\""),
      // Space after inner-list item.
      ("signature=(\"unencoded-digest\";sf );keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\""),
      // Trailing space.
      ("signature=(\"unencoded-digest\";sf);keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"ed25519-integrity\" "),
      // All valid spaces.
      (" signature=( \"unencoded-digest\"; sf );  keyid="
       "\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"; "
       "tag=\"ed25519-integrity\"  ")};

  for (auto* const test : cases) {
    SCOPED_TRACE(test);
    auto headers = ValidHeadersPlusInput(test);
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    std::string expected_base =
        base::StrCat({"\"unencoded-digest\";sf: ", kValidDigestHeader,
                      "\n\"@signature-params\": "
                      "(\"unencoded-digest\";sf);keyid=\"",
                      kPublicKey, "\";tag=\"ed25519-integrity\""});
    EXPECT_EQ(expected_base, result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, AuthorityComponent) {
  struct {
    std::string_view url;
    std::string_view authority;
  } cases[] = {
      {"https://url.test/", "url.test"},
      {"https://url.test/?a", "url.test"},
      {"https://url.test:443/", "url.test"},
      {"https://url.test:444/", "url.test:444"},
      {"http://url.test:80", "url.test"},
      {"http://url.test:81", "url.test:81"},
      {"http://URL.test", "url.test"},
      {"http://ürl.test", "xn--rl-wka.test"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@authority\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@authority\";req: " << test.authority << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf "
                     "\"@authority\";req);"
                  << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_ = CreateRequest(*context_, test.url);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, QueryComponent) {
  struct {
    std::string_view url;
    std::string_view query;
  } cases[] = {
      {"https://url.test/", "?"},
      {"https://url.test/?a", "?a"},
      {"https://url.test/?a=b", "?a=b"},
      {"https://url.test/?a=b&c=d", "?a=b&c=d"},
      {"https://url.test/?a=%2F", "?a=%2F"},
      {"https://url.test/?a=ü", "?a=%C3%BC"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@query\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@query\";req: " << test.query << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@query\";req);"
        << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_ = CreateRequest(*context_, test.url);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, QueryParamComponent) {
  struct {
    std::string_view url;
    std::string_view query;
  } cases[] = {
      {"https://url.test/?a", ""},
      {"https://url.test/?a=b", "b"},
      {"https://url.test/?a=b&c=d", "b"},
      {"https://url.test/?a=/", "%2F"},
      {"https://url.test/?a=%2F", "%2F"},
      {"https://url.test/?a=ü", "%C3%BC"},
      {"https://url.test/?a=percent encoded spaces",
       "percent%20encoded%20spaces"},
      {"https://url.test/?a=percent%20encoded%20spaces",
       "percent%20encoded%20spaces"},
      {"https://url.test/?a=percent+encoded+spaces",
       "percent%20encoded%20spaces"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    // `name`, then `req`
    {
      std::string input_header = base::StrCat(
          {"signature=(\"unencoded-digest\";sf "
           "\"@query-param\";name=\"a\";req);",
           "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

      std::stringstream expected_base;
      expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                    << "\"@query-param\";name=\"a\";req: " << test.query << '\n'
                    << "\"@signature-params\": (\"unencoded-digest\";sf "
                       "\"@query-param\";name=\"a\";req);"
                    << "keyid=\"" << kPublicKey
                    << "\";tag=\"ed25519-integrity\"";

      auto headers = ValidHeadersPlusInput(input_header.c_str());
      auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
      ASSERT_EQ(1u, parsed->signatures.size()) << parsed->issues[0]->error;
      EXPECT_EQ(0u, parsed->issues.size());

      request_ = CreateRequest(*context_, test.url);
      std::optional<std::string> result =
          ConstructSignatureBase(parsed->signatures[0], request(), *headers);
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(expected_base.str(), result.value())
          << GURL(test.url).GetQuery();
    }

    // `req`, then `name`
    {
      std::string input_header = base::StrCat(
          {"signature=(\"unencoded-digest\";sf "
           "\"@query-param\";req;name=\"a\");",
           "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

      std::stringstream expected_base;
      expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                    << "\"@query-param\";req;name=\"a\": " << test.query << '\n'
                    << "\"@signature-params\": (\"unencoded-digest\";sf "
                       "\"@query-param\";req;name=\"a\");"
                    << "keyid=\"" << kPublicKey
                    << "\";tag=\"ed25519-integrity\"";

      auto headers = ValidHeadersPlusInput(input_header.c_str());
      auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
      ASSERT_EQ(1u, parsed->signatures.size()) << parsed->issues[0]->error;
      EXPECT_EQ(0u, parsed->issues.size());

      request_ = CreateRequest(*context_, test.url);
      std::optional<std::string> result =
          ConstructSignatureBase(parsed->signatures[0], request(), *headers);
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(expected_base.str(), result.value())
          << GURL(test.url).GetQuery();
    }
  }
}

TEST_F(SRIMessageSignatureBaseTest, MethodComponent) {
  const char* methods[] = {"GET",     "HEAD",         "POST",    "PUT",
                           "DELETE",  "CONNECT",      "OPTIONS", "TRACE",
                           "UNKNOWN", "CaseSensitive"};
  for (auto* const test_method : methods) {
    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@method\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"sri\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@method\";req: " << test_method << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@method\";req);"
        << "keyid=\"" << kPublicKey << "\";tag=\"sri\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_->set_method(test_method);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, PathComponent) {
  struct {
    std::string_view url;
    std::string_view path;
  } cases[] = {
      {"https://url.test/", "/"},
      {"https://url.test:8080/", "/"},
      {"https://user:pass@url.test/", "/"},
      {"https://url.test/?a=b", "/"},
      {"https://url.test/#anchor", "/"},
      {"https://url.test/path", "/path"},
      {"https://url.test/path/", "/path/"},
      {"https://url.test/pAtH", "/pAtH"},
      {"https://url.test/%0Apath", "/%0Apath"},
      {"https://url.test/%0apath", "/%0apath"},
      {"https://url.test/path/../", "/"},
      {"https://url.test/ü", "/%C3%BC"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@path\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@path\";req: " << test.path << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@path\";req);"
        << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_ = CreateRequest(*context_, test.url);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, TargetUriComponent) {
  struct {
    std::string_view url;
    std::string_view target;
  } cases[] = {
      {"https://url.test/", "https://url.test/"},
      {"https://url.test:8080/", "https://url.test:8080/"},
      {"https://user:pass@url.test/", "https://url.test/"},
      {"https://url.test/?a=b", "https://url.test/?a=b"},
      {"https://url.test/#anchor", "https://url.test/"},
      {"https://url.test/path", "https://url.test/path"},
      {"https://url.test/path/", "https://url.test/path/"},
      {"https://url.test/pAtH", "https://url.test/pAtH"},
      {"https://url.test/%0Apath", "https://url.test/%0Apath"},
      {"https://url.test/%0apath", "https://url.test/%0apath"},
      {"https://url.test/path/../", "https://url.test/"},
      {"https://url.test/ü", "https://url.test/%C3%BC"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header = base::StrCat(
        {"signature=(\"unencoded-digest\";sf \"@target-uri\";req);", "keyid=\"",
         kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@target-uri\";req: " << test.target << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf "
                     "\"@target-uri\";req);"
                  << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_ = CreateRequest(*context_, test.url);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, SchemeComponent) {
  struct {
    std::string_view url;
    std::string_view scheme;
  } cases[] = {
      {"https://url.test/", url::kHttpsScheme},
      {"HTTPS://url.test/", url::kHttpsScheme},
      {"http://url.test/", url::kHttpScheme},
      {"HTTP://url.test/", url::kHttpScheme},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.url);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@scheme\";req);",
                      "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@scheme\";req: " << test.scheme << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@scheme\";req);"
        << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    request_ = CreateRequest(*context_, test.url);
    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, StatusComponent) {
  for (int i = 0; i < net::HttpStatusCode::HTTP_STATUS_CODE_MAX; i++) {
    std::optional<net::HttpStatusCode> test_code =
        net::TryToGetHttpStatusCode(i);
    if (!test_code) {
      continue;
    }

    SCOPED_TRACE(testing::Message() << "Status code: " << i);

    std::string input_header =
        base::StrCat({"signature=(\"unencoded-digest\";sf \"@status\");",
                      "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

    std::stringstream expected_base;
    expected_base
        << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
        << "\"@status\": " << *test_code << '\n'
        << "\"@signature-params\": (\"unencoded-digest\";sf \"@status\");"
        << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

    auto headers =
        ValidHeadersPlusInputAndStatus(input_header.c_str(), *test_code);
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeaderParams) {
  struct {
    int64_t created;
    int64_t expires;
    std::string nonce;
  } cases[] = {{0, 0, ""},
               {0, 1, ""},
               {0, 0, "noncy-nonce"},
               {0, 1, "noncy-nonce"},
               {1, 0, ""},
               {1, 1, ""},
               {1, 0, "noncy-nonce"},
               {1, 1, "noncy-nonce"},
               {999999999999999, 999999999999999, "noncy-nonce"}};

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << "Test case:\n- Created: `" << test.created
                 << "`\n- Expires: `" << test.expires << "`\n- Nonce:  `"
                 << test.nonce << '`');

    // Construct the header and the expectations based on the test case:
    std::stringstream input_header;
    input_header << "signature=(\"unencoded-digest\";sf)";

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf)";
    if (test.created) {
      input_header << ";created=" << test.created;
      expected_base << ";created=" << test.created;
    }
    if (test.expires) {
      input_header << ";expires=" << test.expires;
      expected_base << ";expires=" << test.expires;
    }
    input_header << ";keyid=\"" << kPublicKey << '"';
    expected_base << ";keyid=\"" << kPublicKey << '"';
    if (!test.nonce.empty()) {
      input_header << ";nonce=\"" << test.nonce << '"';
      expected_base << ";nonce=\"" << test.nonce << '"';
    }
    input_header << ";tag=\"ed25519-integrity\"";
    expected_base << ";tag=\"ed25519-integrity\"";

    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ParameterSorting) {
  std::vector<const char*> params = {
      "created=12345", "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"", "nonce=\"n\"",
      "tag=\"ed25519-integrity\""};

  do {
    std::stringstream input_header;
    input_header << "signature=(\"unencoded-digest\";sf)";

    std::stringstream expected_base;
    expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                  << "\"@signature-params\": (\"unencoded-digest\";sf)";
    for (const char* param : params) {
      input_header << ';' << param;
      expected_base << ';' << param;
    }

    SCOPED_TRACE(input_header.str());
    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    EXPECT_THAT(result, testing::Optional(expected_base.str()));
  } while (std::next_permutation(params.begin(), params.end()));
}

TEST_F(SRIMessageSignatureBaseTest, UnknownParameters) {
  std::vector<const char*> cases = {
      "unknown",        "unknown=1",     "unknown=1.1", "unknown=\"string\"",
      "unknown=:YQ==:", "unknown=token", "unknown=?0",
      // We don't support Date or Display String yet.
      // "unknown=@12345",
      // "unknown=%\"display\"",
  };

  for (auto* const test : cases) {
    SCOPED_TRACE(test);
    std::string test_header =
        base::StrCat({kValidSignatureInputHeader, ";", test});
    auto headers = ValidHeadersPlusInput(test_header.data());
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    std::string expected_base =
        base::StrCat({"\"unencoded-digest\";sf: ", kValidDigestHeader,
                      "\n\"@signature-params\": "
                      "(\"unencoded-digest\";sf);keyid=\"",
                      kPublicKey, "\";tag=\"ed25519-integrity\"", ";", test});
    EXPECT_EQ(expected_base, result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ArbitraryResponseHeaderComponent) {
  const char* kTestHeaderName = "arbitrary-header";
  const char* kTestHeaderValue = "test-value";

  std::string input_header =
      base::StrCat({"signature=(\"unencoded-digest\";sf \"arbitrary-header\");",
                    "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

  std::stringstream expected_base;
  expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                << "\"arbitrary-header\": " << kTestHeaderValue << '\n'
                << "\"@signature-params\": (\"unencoded-digest\";sf "
                   "\"arbitrary-header\");"
                << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

  auto headers = ValidHeadersPlusInput(input_header.c_str());

  // First, verify failure when the header is missing:
  {
    ASSERT_FALSE(headers->HasHeader("arbitrary-header"));

    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    EXPECT_FALSE(result.has_value());
  }

  // Then, add the header and verify success:
  {
    headers->AddHeader(kTestHeaderName, kTestHeaderValue);

    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    std::optional<std::string> result =
        ConstructSignatureBase(parsed->signatures[0], request(), *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, BinaryWrappedComponent) {
  const char* kTestHeaderName = "arbitrary-header";
  const char* kTestHeaderValue = "test-value";

  std::string input_header = base::StrCat(
      {"signature=(\"unencoded-digest\";sf \"arbitrary-header\";bs);",
       "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

  std::stringstream expected_base;
  expected_base << "\"unencoded-digest\";sf: " << kValidDigestHeader << '\n'
                << "\"arbitrary-header\";bs: :"
                << base::Base64Encode(kTestHeaderValue) << ":\n"
                << "\"@signature-params\": (\"unencoded-digest\";sf "
                   "\"arbitrary-header\";bs);"
                << "keyid=\"" << kPublicKey << "\";tag=\"ed25519-integrity\"";

  auto headers = ValidHeadersPlusInput(input_header.c_str());
  headers->AddHeader(kTestHeaderName, kTestHeaderValue);

  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  std::optional<std::string> result =
      ConstructSignatureBase(parsed->signatures[0], request(), *headers);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(expected_base.str(), result.value());
}

//
// Validation Tests
//
class SRIMessageSignatureValidationTest : public testing::Test {
 protected:
  SRIMessageSignatureValidationTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()),
        request_(CreateRequest(*context_, kExampleURL)) {}

  const net::URLRequest& request() { return *request_; }

  scoped_refptr<net::HttpResponseHeaders> Headers(std::string_view digest,
                                                  std::string_view signature,
                                                  std::string_view input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (!digest.empty()) {
      builder.AddHeader("Unencoded-Digest", digest);
    }
    if (!signature.empty()) {
      builder.AddHeader("Signature", signature);
    }
    if (!input.empty()) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }

  scoped_refptr<net::HttpResponseHeaders> ValidHeaders() {
    return Headers(kValidDigestHeader, kValidSignatureHeader,
                   kValidSignatureInputHeader);
  }

  std::string SignatureInputHeader(std::string_view name,
                                   std::string_view keyid) {
    return base::StrCat({name,
                         "=(\"unencoded-digest\";sf);"
                         "keyid=\"",
                         keyid, "\";tag=\"ed25519-integrity\""});
  }

  std::string SignatureHeader(std::string name, std::string_view sig) {
    return base::StrCat({name, "=:", sig, ":"});
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> request_;
};

TEST_F(SRIMessageSignatureValidationTest, NoSignatures) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(0u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  EXPECT_EQ(0u, parsed->issues.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignature) {
  auto headers = ValidHeaders();
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  EXPECT_EQ(0u, parsed->issues.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidPlusInvalidSignature) {
  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  std::string signature_header =
      base::StrCat({SignatureHeader("signature", kSignature), ",",
                    SignatureHeader("bad-signature", wrong_signature)});
  std::string input_header =
      base::StrCat({SignatureInputHeader("signature", kPublicKey), ",",
                    SignatureInputHeader("bad-signature", wrong_key)});
  auto headers = Headers(kValidDigestHeader, signature_header, input_header);

  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  EXPECT_FALSE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  ASSERT_EQ(1u, parsed->issues.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch,
            parsed->issues[0]->error);
}

TEST_F(SRIMessageSignatureValidationTest, MultipleValidSignatures) {
  std::string signature_header =
      base::StrCat({SignatureHeader("signature", kSignature), ",",
                    SignatureHeader("bad-signature", kSignature)});
  std::string input_header =
      base::StrCat({SignatureInputHeader("signature", kPublicKey), ",",
                    SignatureInputHeader("bad-signature", kPublicKey)});
  auto headers = Headers(kValidDigestHeader, signature_header, input_header);

  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  EXPECT_EQ(0u, parsed->issues.size());
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignatureExpires) {
  auto headers = Headers(kValidDigestHeader, kValidExpiringSignatureHeader,
                         kValidExpiringSignatureInputHeader);
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  // Signature should validate at the moment before and of expiration.
  auto diff = kValidExpiringSignatureExpiresAt -
              base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000 - 1;
  task_environment_.AdvanceClock(base::Seconds(diff));
  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  ASSERT_EQ(0u, parsed->issues.size());

  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  ASSERT_EQ(0u, parsed->issues.size());

  // ...but not after expiration.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_FALSE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  ASSERT_EQ(1u, parsed->issues.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kValidationFailedSignatureExpired,
            parsed->issues[0]->error);
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignatureDigestHeaderMismatch) {
  const char* cases[] = {
      "",
      "sha-256=:YQ==:",
      kValidDigestHeader512,
  };

  for (auto* test : cases) {
    SCOPED_TRACE(testing::Message() << "Test case: `" << test << '`');

    auto headers =
        Headers(test, kValidSignatureHeader, kValidSignatureInputHeader);
    auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, parsed->signatures.size());
    EXPECT_EQ(0u, parsed->issues.size());

    EXPECT_FALSE(
        ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
    EXPECT_EQ(1u, parsed->issues.size());
    EXPECT_EQ(
        mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch,
        parsed->issues[0]->error);
  }
}

TEST_F(SRIMessageSignatureValidationTest, MissingHeader) {
  // This signature is valid for a base that includes "unencoded-digest".
  // We'll try to validate it against a signature declaration that also
  // includes "x-test-header". The signature base will be different, so
  // validation should fail.
  std::string input_header =
      base::StrCat({"signature=(\"unencoded-digest\";sf \"x-test-header\");",
                    "keyid=\"", kPublicKey, "\";tag=\"ed25519-integrity\""});

  auto headers =
      Headers(kValidDigestHeader, kValidSignatureHeader, input_header);
  // Not adding x-test-header.
  auto parsed = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, parsed->signatures.size());
  EXPECT_EQ(0u, parsed->issues.size());

  EXPECT_FALSE(
      ValidateSRIMessageSignaturesOverHeaders(parsed, request(), *headers));
  ASSERT_EQ(1u, parsed->issues.size());
  EXPECT_EQ(mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch,
            parsed->issues[0]->error);
}

class SRIMessageSignatureEnforcementTest
    : public SRIMessageSignatureValidationTest {
 protected:
  SRIMessageSignatureEnforcementTest() {}

  mojom::URLResponseHeadPtr ResponseHead(std::string_view digest,
                                         std::string_view signature,
                                         std::string_view input) {
    auto head = mojom::URLResponseHead::New();
    head->headers = Headers(digest, signature, input);
    return head;
  }
};

TEST_F(SRIMessageSignatureEnforcementTest, NoHeaders) {
  auto head = ResponseHead("", "", "");
  auto result = MaybeBlockResponseForSRIMessageSignature(request(), *head, {});
  EXPECT_FALSE(result.has_value());
}

TEST_F(SRIMessageSignatureEnforcementTest, ValidHeaders) {
  auto head = ResponseHead(kValidDigestHeader, kValidSignatureHeader,
                           kValidSignatureInputHeader);
  auto result = MaybeBlockResponseForSRIMessageSignature(request(), *head, {});
  EXPECT_FALSE(result.has_value());
}

TEST_F(SRIMessageSignatureEnforcementTest, ValidHeadersWithMatchingIntegrity) {
  auto head = ResponseHead(kValidDigestHeader, kValidSignatureHeader,
                           kValidSignatureInputHeader);

  const std::vector<uint8_t> public_key = *base::Base64Decode(kPublicKey);

  // Matching key.
  {
    auto result = MaybeBlockResponseForSRIMessageSignature(request(), *head,
                                                           {public_key});
    EXPECT_FALSE(result.has_value());
  }

  // Matching key + non-matching key.
  std::string wrong_key_str = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const std::vector<uint8_t> wrong_key = *base::Base64Decode(wrong_key_str);
  {
    auto result = MaybeBlockResponseForSRIMessageSignature(
        request(), *head, {public_key, wrong_key});
    EXPECT_FALSE(result.has_value());
  }

  // Non-matching key + matching key.
  {
    auto result = MaybeBlockResponseForSRIMessageSignature(
        request(), *head, {wrong_key, public_key});
    EXPECT_FALSE(result.has_value());
  }
}

TEST_F(SRIMessageSignatureEnforcementTest,
       ValidHeadersWithMismatchedIntegrity) {
  std::string wrong_key_str = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  auto wrong_key = *base::Base64Decode(wrong_key_str);

  auto head = ResponseHead(kValidDigestHeader, kValidSignatureHeader,
                           kValidSignatureInputHeader);
  auto result =
      MaybeBlockResponseForSRIMessageSignature(request(), *head, {wrong_key});

  // Regardless of the feature-flag's state, integrity requirements are
  // enforced.
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
            result.value());
}

TEST_F(SRIMessageSignatureEnforcementTest, MismatchedHeaders) {
  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  auto head = ResponseHead(kValidDigestHeader,
                           SignatureHeader("bad-signature", wrong_signature),
                           SignatureInputHeader("bad-signature", wrong_key));
  auto result = MaybeBlockResponseForSRIMessageSignature(request(), *head, {});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
            result.value());
}

TEST_F(SRIMessageSignatureEnforcementTest, MismatchedHeadersAndForcedChecks) {
  // Same test as `MismatchedHeaders`, but with a key expectation. It should
  // still consistently fail.
  const char* wrong_key_str = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  auto wrong_key = *base::Base64Decode(wrong_key_str);

  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  auto head = ResponseHead(
      kValidDigestHeader, SignatureHeader("bad-signature", wrong_signature),
      SignatureInputHeader("bad-signature", wrong_key_str));
  auto result =
      MaybeBlockResponseForSRIMessageSignature(request(), *head, {wrong_key});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
            result.value());
}

class SRIMessageSignatureRequestHeaderTest : public testing::Test {
 public:
  SRIMessageSignatureRequestHeaderTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        context_(net::CreateTestURLRequestContextBuilder()->Build()),
        url_request_(context_->CreateRequest(kExampleURL,
                                             net::DEFAULT_PRIORITY,
                                             /*delegate=*/nullptr,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)) {}

  net::URLRequest* url_request() const { return url_request_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> url_request_;
};

TEST_F(SRIMessageSignatureRequestHeaderTest, NoSignaturesNoHeader) {
  MaybeSetAcceptSignatureHeader(url_request(), {});
  EXPECT_FALSE(url_request()
                   ->extra_request_headers()
                   .GetHeader(kAcceptSignature)
                   .has_value());
}

// Most invalid data is filtered out by the binary encoding: really all that's
// left is to ensure that the public key is the proper length for ed25519.
TEST_F(SRIMessageSignatureRequestHeaderTest, InvalidSignatures) {
  MaybeSetAcceptSignatureHeader(url_request(), {*base::Base64Decode("YQ==")});
  EXPECT_FALSE(url_request()
                   ->extra_request_headers()
                   .GetHeader(kAcceptSignature)
                   .has_value());
}

TEST_F(SRIMessageSignatureRequestHeaderTest, ValidSignatures) {
  const std::vector<uint8_t> public_key = *base::Base64Decode(kPublicKey);
  const std::vector<uint8_t> public_key2 = *base::Base64Decode(kPublicKey2);

  // One valid signature:
  {
    MaybeSetAcceptSignatureHeader(url_request(), {public_key});
    auto result =
        url_request()->extra_request_headers().GetHeader(kAcceptSignature);
    std::string expected =
        base::StrCat({"sig0=(\"unencoded-digest\";sf);keyid=\"", kPublicKey,
                      "\";tag=\"ed25519-integrity\""});
    EXPECT_THAT(result, testing::Optional(expected));
  }

  // Two valid signature:
  {
    MaybeSetAcceptSignatureHeader(url_request(), {public_key, public_key2});
    auto result =
        url_request()->extra_request_headers().GetHeader(kAcceptSignature);
    std::string expected =
        base::StrCat({"sig0=(\"unencoded-digest\";sf);keyid=\"", kPublicKey,
                      "\";tag=\"ed25519-integrity\", ",
                      "sig1=(\"unencoded-digest\";sf);keyid=\"", kPublicKey2,
                      "\";tag=\"ed25519-integrity\""});
    EXPECT_THAT(result, testing::Optional(expected));
  }

  // Two valid signature, order matters:
  {
    MaybeSetAcceptSignatureHeader(url_request(), {public_key2, public_key});
    auto result =
        url_request()->extra_request_headers().GetHeader(kAcceptSignature);
    std::string expected =
        base::StrCat({"sig0=(\"unencoded-digest\";sf);keyid=\"", kPublicKey2,
                      "\";tag=\"ed25519-integrity\", ",
                      "sig1=(\"unencoded-digest\";sf);keyid=\"", kPublicKey,
                      "\";tag=\"ed25519-integrity\""});
    EXPECT_THAT(result, testing::Optional(expected));
  }
}

}  // namespace network
