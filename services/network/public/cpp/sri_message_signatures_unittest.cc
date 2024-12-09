// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/sri_message_signature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

using Parameters = mojom::SRIMessageSignatureComponent::Parameter;

namespace {

// Exciting test constants, leaning on test data from the RFC.
//
// Base64 encoded Ed25519 Test Keys, pulled from the RFC at
// https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4
const char* kPublicKey = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";

// The following constants are extracted from this known-good response that
// matches the constraints described in
// https://wicg.github.io/signature-based-sri/#verification-requirements-for-sri
//
// ```
// HTTP/1.1 200 OK
// Date: Tue, 20 Apr 2021 02:07:56 GMT
// Content-Type: application/json
// Identity-Digest: sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:
// Content-Length: 18
// Signature-Input:
// signature=("identity-digest";sf);alg="ed25519";keyid="JrQLj5P/89iXES9+vFgrI \
//           y29clF9CC/oPPsw3c5D0bs=";tag="sri"
// Signature: signature=:H7AqWWgo1DJ7VdyF9DKotG/4hvatKDfRTq2mpuY/hvJupSn+EYzus \
//            5p24qPK7DtVQcxJFhzSYDj4RBq9grZTAQ==:
//
// {"hello": "world"}
// ```
const char* kSignature =
    "H7AqWWgo1DJ7VdyF9DKotG/4hvatKDfRTq2mpuY/hvJupSn+EYzus"
    "5p24qPK7DtVQcxJFhzSYDj4RBq9grZTAQ==";

const char* kValidDigestHeader =
    "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
const char* kValidDigestHeader512 =
    "sha-512=:WZDPaVn/7XgHaAy8pmojAkGWoRx2UFChF41A2svX+TaPm+AbwAgBWnrIiYllu7BNN"
    "yealdVLvRwEmTHWXvJwew==:";

// A basic signature header set with no expiration.
const char* kValidSignatureInputHeader =
    "signature=(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
    "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
const char* kValidSignatureHeader =
    "signature=:H7AqWWgo1DJ7VdyF9DKotG/4hvatKDfRTq2mpuY/hvJupSn+EYzus5p24qPK7Dt"
    "VQcxJFhzSYDj4RBq9grZTAQ==:";

// The following signature was generated using test-key-ed25519 from RFC 9421
// (https://datatracker.ietf.org/doc/html/rfc9421#appendix-B.1.4),
// the same key used for generating the constants above.
//
// A valid signature header set with expiration in the future (2142-12-30).
const char* kValidExpiringSignatureInputHeader =
    "signature=(\"identity-digest\";sf);alg=\"ed25519\";expires=5459212800;"
    "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
const char* kValidExpiringSignatureHeader =
    "signature=:oVJa+A12xhF1hJz1IMLY6e8fap3uFVJbnhNi6vSYSVnYpZtUUGjtYtNZpqm"
    "VnflfJAbkqCV7Llh842pv8SBIAg==:";
const int64_t kValidExpiringSignatureExpiresAt = 5459212800;

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

  void ValidateBasicTestHeader(const mojom::SRIMessageSignaturePtr& sig) {
    EXPECT_EQ("signature", sig->label);
    EXPECT_EQ(mojom::SRIMessageSignature::Algorithm::kEd25519, sig->alg);
    EXPECT_EQ(std::nullopt, sig->created);
    EXPECT_EQ(std::nullopt, sig->expires);
    EXPECT_EQ(kPublicKey, sig->keyid);
    EXPECT_EQ(std::nullopt, sig->nonce);
    EXPECT_EQ("sri", sig->tag);
    EXPECT_EQ(kSignature, base::Base64Encode(sig->signature));

    ASSERT_EQ(1u, sig->components.size());
    EXPECT_EQ("identity-digest", sig->components[0]->name);
    ASSERT_EQ(1u, sig->components[0]->params.size());
    EXPECT_EQ(mojom::SRIMessageSignatureComponent::Parameter::
                  kStrictStructuredFieldSerialization,
              sig->components[0]->params[0]);
  }
};

TEST_F(SRIMessageSignatureParserTest, NoHeaders) {
  auto headers = GetHeaders(/*signature=*/nullptr, /*input=*/nullptr);
  std::vector<mojom::SRIMessageSignaturePtr> result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result.size());
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureHeader) {
  auto headers = GetHeaders(/*signature=*/nullptr, kValidSignatureInputHeader);
  std::vector<mojom::SRIMessageSignaturePtr> result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result.size());
}

TEST_F(SRIMessageSignatureParserTest, NoSignatureInputHeader) {
  auto headers = GetHeaders(kValidSignatureHeader, /*input=*/nullptr);
  std::vector<mojom::SRIMessageSignaturePtr> result =
      ParseSRIMessageSignaturesFromHeaders(*headers);
  EXPECT_EQ(0u, result.size());
}

TEST_F(SRIMessageSignatureParserTest, ValidHeaders) {
  auto headers = GetHeaders(kValidSignatureHeader, kValidSignatureInputHeader);
  std::vector<mojom::SRIMessageSignaturePtr> result =
      ParseSRIMessageSignaturesFromHeaders(*headers);

  EXPECT_EQ(1u, result.size());
  ValidateBasicTestHeader(result[0]);
}

TEST_F(SRIMessageSignatureParserTest, UnmatchedLabelsInAdditionToValidHeaders) {
  // kValidSignatureInputHeader defines inputs for the `signature` label. The
  // following header will define a signature for that label, as well as another
  // signature with an unused label.
  //
  // We're currently ignoring this mismatched signature, and therefore treating
  // the header as valid.
  std::string two_signatures =
      base::StrCat({"unused=:badbeef:,", kValidSignatureHeader});
  std::string two_inputs = base::StrCat(
      {"also-unused=(\"arbitrary\" \"data\"),", kValidSignatureInputHeader});

  // Too many signatures:
  {
    auto headers =
        GetHeaders(two_signatures.c_str(), kValidSignatureInputHeader);
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result.size());
    ValidateBasicTestHeader(result[0]);
  }

  // Too many inputs:
  {
    auto headers = GetHeaders(kValidSignatureHeader, two_inputs.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result.size());
    ValidateBasicTestHeader(result[0]);
  }

  // Too many everythings!
  {
    auto headers = GetHeaders(two_signatures.c_str(), two_inputs.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);
    EXPECT_EQ(1u, result.size());
    ValidateBasicTestHeader(result[0]);
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
      "key=",
      "key=1",
      "key=1.1",
      "key=\"string\"",
      "key=token",
      "key=?0",
      "key=@12345",
      "key=%\"display\"",
      "key=(inner list of tokens)",

      // Dictionaries with byte-sequence values of the wrong length:
      "key=:YQ==:",

      // Parameterized, but otherwise correct byte-sequence values:
      ("key=:amDAmvl9bsfIcfA/bIJsBuBvInjJAaxxNIlLOzNI3FkrnG2k52UxXJprz89+2aO"
       "wEAz3w6KjjZuGkdrOUwxhBQ==:;param=1"),
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    auto headers = GetHeaders(test, kValidSignatureInputHeader);
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputComponents) {
  const char* cases[] = {
      // Non-dictionaries:
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

      // Dictionaries with non-inner-list values:
      "signature=",
      "signature=1",
      "signature=1.1",
      "signature=\"string\"",
      "signature=token",
      "signature=?0",
      "signature=@12345",
      "signature=%\"display\"",
      "signature=:badbeef:",

      // Dictionaries with inner-list values that contain non-strings:
      "signature=()",
      "signature=(1)",
      "signature=(1.1)",
      "signature=(token)",
      "signature=(:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:)",
      "signature=(?0)",
      "signature=(@12345)",
      "signature=(%\"display\")",
      "signature=(A, list, of, tokens)",
      "signature=(\"invalid header names\")",
      "signature=(\"@unknown-derived-components\")",

      // Components that are valid per-spec, but aren't quite constrained enough
      // for SRI's initial implementation. We'll eventually treat these as valid
      // headers, but they're parse errors for now.
      "signature=(\"not-identity-digest\")",
      "signature=(\"Identity-Digest\")",
      "signature=(\"IDENTITY-DIGEST\")",
      "signature=(\"identity-digest\" \"and-something-else\")",
      "signature=(\"something-else\" \"identity-digest\")",

      // Invalid component params:
      "signature=(\"identity-digest\")",
      "signature=(\"identity-digest\";sf=)",
      "signature=(\"identity-digest\";sf=1)",
      "signature=(\"identity-digest\";sf=1.1)",
      "signature=(\"identity-digest\";sf=\"string\")",
      "signature=(\"identity-digest\";sf=token)",
      "signature=(\"identity-digest\";sf=?0)",
      "signature=(\"identity-digest\";sf=@12345)",
      "signature=(\"identity-digest\";sf=%\"display\")",
      "signature=(\"identity-digest\";sf=:badbeef:)",
      "signature=(\"identity-digest\";sf;not-sf)",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");

    // Tack valid parameters onto the test string so that we're actually
    // just testing the component parsing.
    std::string test_with_params = base::StrCat(
        {test, ";alg=\"ed25519\";keyid=\"", kPublicKey, "\";tag=\"sri\""});
    auto headers = GetHeaders(kValidSignatureHeader, test_with_params.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result.size());
  }
}

TEST_F(SRIMessageSignatureParserTest, MalformedSignatureInputParameters) {
  const char* cases[] = {
      // Missing a required parameter:
      "alg=\"ed25519\"",
      "alg=\"ed25519\";keyid=\"[KEY]\"",
      "alg=\"ed25519\";tag=\"sri\"",
      "keyid=\"[KEY]\"",
      "keyid=\"[KEY]\";tag=\"sri\"",
      "tag=\"sri\"",

      // Duplication (insofar as the invalid value comes last):
      "alg=\"ed25519\";alg=\"not-ed25519\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";keyid=\"not-[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=\"sri\";tag=\"not-sri\"",

      // Unknown parameter:
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=\"sri\";unknown=1",

      // Invalid alg:
      //
      // - Types:
      "alg=;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=token;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      // - Values
      "alg=\"not-ed25519\";keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `created`:
      //
      // - Types:
      "alg=\"ed25519\";created=;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=\"string\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=token;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";created=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      // - Values
      "alg=\"ed25519\";created=-1;keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `expires`:
      //
      // - Types:
      "alg=\"ed25519\";expires=;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=1.1;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=\"string\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=token;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=?0;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=@12345;keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=%\"display\";keyid=\"[KEY]\";tag=\"sri\"",
      "alg=\"ed25519\";expires=:badbeef:;keyid=\"[KEY]\";tag=\"sri\"",
      // - Values
      "alg=\"ed25519\";expires=-1;keyid=\"[KEY]\";tag=\"sri\"",

      // Invalid `keyid`:
      //
      // - Types
      "alg=\"ed25519\";keyid=;tag=\"sri\"",
      "alg=\"ed25519\";keyid=1;tag=\"sri\"",
      "alg=\"ed25519\";keyid=1.1;tag=\"sri\"",
      "alg=\"ed25519\";keyid=token;tag=\"sri\"",
      "alg=\"ed25519\";keyid=?0;tag=\"sri\"",
      "alg=\"ed25519\";keyid=@12345;tag=\"sri\"",
      "alg=\"ed25519\";keyid=%\"display\";tag=\"sri\"",
      "alg=\"ed25519\";keyid=:badbeef:;tag=\"sri\"",
      // - Values
      "alg=\"ed25519\";keyid=\"not a base64-encoded key\";tag=\"sri\"",

      // Invalid `nonce`:
      //
      // - Types
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=1;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=1.1;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=token;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=?0;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=@12345;tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=%\"display\";tag=\"not-sri\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";nonce=:badbeef:;tag=\"not-sri\"",

      // Invalid `tag`:
      //
      // - Types
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=1",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=1.1",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=token",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=?0",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=@12345",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=%\"display\"",
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=:badbeef:",
      // - Values
      "alg=\"ed25519\";keyid=\"[KEY]\";tag=\"not-sri\"",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    std::string processed_input =
        base::StrCat({"signature=(\"identity-digest\";sf);", test});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    // As these are all malformed, we expect parsing to return no headers.
    EXPECT_EQ(0u, result.size());
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
    std::string processed_input = base::StrCat(
        {"signature=(\"identity-digest\";sf);alg=\"ed25519\";created=", test,
         ";keyid=\"[KEY]\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result.size());
    ASSERT_TRUE(result[0]->created.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result[0]->created.value());
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
    std::string processed_input = base::StrCat(
        {"signature=(\"identity-digest\";sf);alg=\"ed25519\";expires=", test,
         ";keyid=\"[KEY]\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result.size());
    ASSERT_TRUE(result[0]->expires.has_value());

    int64_t expected_int;
    base::StringToInt64(test, &expected_int);
    EXPECT_EQ(expected_int, result[0]->expires.value());
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
    std::string processed_input = base::StrCat(
        {"signature=(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"[KEY]\";",
         "nonce=\"", test, "\";tag=\"sri\""});
    size_t key_pos = processed_input.find("[KEY]");
    if (key_pos != std::string::npos) {
      processed_input.replace(key_pos, 5, kPublicKey);
    }
    auto headers = GetHeaders(kValidSignatureHeader, processed_input.c_str());
    std::vector<mojom::SRIMessageSignaturePtr> result =
        ParseSRIMessageSignaturesFromHeaders(*headers);

    ASSERT_EQ(1u, result.size());
    ASSERT_TRUE(result[0]->nonce.has_value());
    EXPECT_EQ(test, result[0]->nonce.value());
  }
}

TEST_F(SRIMessageSignatureParserTest, ParameterSorting) {
  std::vector<const char*> params = {
      "alg=\"ed25519\"",
      "created=12345",
      "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"",
      "nonce=\"n\"",
      "tag=\"sri\""};

  do {
    std::stringstream header;
    header << "signature=(\"identity-digest\";sf)";
    for (const char* param : params) {
      header << ';' << param;
    }
    SCOPED_TRACE(header.str());
    auto headers = GetHeaders(kValidSignatureHeader, header.str().c_str());
    auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, signatures.size());
  } while (std::next_permutation(params.begin(), params.end()));
}

//
// "Signature Base" Creation Tests
//
class SRIMessageSignatureBaseTest : public testing::Test {
 protected:
  SRIMessageSignatureBaseTest() {}

  scoped_refptr<net::HttpResponseHeaders> ValidHeadersPlusInput(
      const char* input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    builder.AddHeader("Identity-Digest", kValidDigestHeader);
    builder.AddHeader("Signature", kValidSignatureHeader);
    if (input) {
      builder.AddHeader("Signature-Input", input);
    }
    return builder.Build();
  }
};

TEST_F(SRIMessageSignatureBaseTest, NoSignaturesNoBase) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  mojom::SRIMessageSignaturePtr signature;

  std::optional<std::string> result =
      ConstructSignatureBase(signature, *headers);
  EXPECT_FALSE(result.has_value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersValidBase) {
  auto headers = ValidHeadersPlusInput(kValidSignatureInputHeader);
  auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, signatures.size());

  std::optional<std::string> result =
      ConstructSignatureBase(signatures[0], *headers);
  ASSERT_TRUE(result.has_value());
  std::string expected_base =
      base::StrCat({"\"identity-digest\": ", kValidDigestHeader,
                    "\n\"@signature-params\": "
                    "(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"",
                    kPublicKey, "\";tag=\"sri\""});
  EXPECT_EQ(expected_base, result.value());
}

TEST_F(SRIMessageSignatureBaseTest, ValidHeadersStrictlySerializedBase) {
  // Regardless of (valid) whitespace, the signature base is strictly
  // serialized.
  const char* cases[] = {
      // Base
      ("signature=(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Leading space.
      (" signature=(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space before inner-list item.
      ("signature=( \"identity-digest\";sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space after `;` in a param.
      ("signature=(\"identity-digest\"; sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Space after inner-list item.
      ("signature=(\"identity-digest\";sf );alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\""),
      // Trailing space.
      ("signature=(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"JrQLj5P/"
       "89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\" "),
      // All valid spaces.
      (" signature=( \"identity-digest\"; sf ); alg=\"ed25519\"; keyid="
       "\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"; tag=\"sri\"  ")};

  for (auto* const test : cases) {
    SCOPED_TRACE(test);
    auto headers = ValidHeadersPlusInput(test);
    auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, signatures.size());

    std::optional<std::string> result =
        ConstructSignatureBase(signatures[0], *headers);
    ASSERT_TRUE(result.has_value());
    std::string expected_base =
        base::StrCat({"\"identity-digest\": ", kValidDigestHeader,
                      "\n\"@signature-params\": "
                      "(\"identity-digest\";sf);alg=\"ed25519\";keyid=\"",
                      kPublicKey, "\";tag=\"sri\""});
    EXPECT_EQ(expected_base, result.value());
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
    input_header << "signature=(\"identity-digest\";sf);alg=\"ed25519\"";

    std::stringstream expected_base;
    expected_base
        << "\"identity-digest\": " << kValidDigestHeader << '\n'
        << "\"@signature-params\": (\"identity-digest\";sf);alg=\"ed25519\"";
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
    input_header << ";tag=\"sri\"";
    expected_base << ";tag=\"sri\"";

    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, signatures.size());

    std::optional<std::string> result =
        ConstructSignatureBase(signatures[0], *headers);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(expected_base.str(), result.value());
  }
}

TEST_F(SRIMessageSignatureBaseTest, ParameterSorting) {
  std::vector<const char*> params = {
      "alg=\"ed25519\"",
      "created=12345",
      "expires=12345",
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\"",
      "nonce=\"n\"",
      "tag=\"sri\""};

  do {
    std::stringstream input_header;
    input_header << "signature=(\"identity-digest\";sf)";

    std::stringstream expected_base;
    expected_base << "\"identity-digest\": " << kValidDigestHeader << '\n'
                  << "\"@signature-params\": (\"identity-digest\";sf)";
    for (const char* param : params) {
      input_header << ';' << param;
      expected_base << ';' << param;
    }

    SCOPED_TRACE(input_header.str());
    auto headers = ValidHeadersPlusInput(input_header.str().c_str());
    auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, signatures.size());

    std::optional<std::string> result =
        ConstructSignatureBase(signatures[0], *headers);
    EXPECT_THAT(result, testing::Optional(expected_base.str()));
  } while (std::next_permutation(params.begin(), params.end()));
}

//
// Validation Tests
//
class SRIMessageSignatureValidationTest : public testing::Test {
 protected:
  SRIMessageSignatureValidationTest() {}

  scoped_refptr<net::HttpResponseHeaders> Headers(std::string_view digest,
                                                  std::string_view signature,
                                                  std::string_view input) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (!digest.empty()) {
      builder.AddHeader("Identity-Digest", digest);
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
                         "=(\"identity-digest\";sf);alg=\"ed25519\";"
                         "keyid=\"",
                         keyid, "\";tag=\"sri\""});
  }

  std::string SignatureHeader(std::string name, std::string_view sig) {
    return base::StrCat({name, "=:", sig, ":"});
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SRIMessageSignatureValidationTest, NoSignatures) {
  auto headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200").Build();
  std::vector<mojom::SRIMessageSignaturePtr> signatures;

  EXPECT_TRUE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignature) {
  auto headers = ValidHeaders();
  auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, signatures.size());

  EXPECT_TRUE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
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

  auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, signatures.size());

  EXPECT_FALSE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
}

TEST_F(SRIMessageSignatureValidationTest, MultipleValidSignatures) {
  std::string signature_header =
      base::StrCat({SignatureHeader("signature", kSignature), ",",
                    SignatureHeader("bad-signature", kSignature)});
  std::string input_header =
      base::StrCat({SignatureInputHeader("signature", kPublicKey), ",",
                    SignatureInputHeader("bad-signature", kPublicKey)});
  auto headers = Headers(kValidDigestHeader, signature_header, input_header);

  auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(2u, signatures.size());

  EXPECT_TRUE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
}

TEST_F(SRIMessageSignatureValidationTest, ValidSignatureExpires) {
  auto headers = Headers(kValidDigestHeader, kValidExpiringSignatureHeader,
                         kValidExpiringSignatureInputHeader);
  auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
  ASSERT_EQ(1u, signatures.size());

  // Signature should validate at the moment before and of expiration.
  auto diff = kValidExpiringSignatureExpiresAt -
              base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000 - 1;
  task_environment_.AdvanceClock(base::Seconds(diff));
  EXPECT_TRUE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_TRUE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));

  // ...but not after expiration.
  task_environment_.AdvanceClock(base::Seconds(1));
  EXPECT_FALSE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
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
    auto signatures = ParseSRIMessageSignaturesFromHeaders(*headers);
    ASSERT_EQ(1u, signatures.size());

    EXPECT_FALSE(ValidateSRIMessageSignaturesOverHeaders(signatures, *headers));
  }
}

class SRIMessageSignatureEnforcementTest
    : public SRIMessageSignatureValidationTest,
      public testing::WithParamInterface<bool> {
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

INSTANTIATE_TEST_SUITE_P(FeatureFlag,
                         SRIMessageSignatureEnforcementTest,
                         testing::Values(true, false));

TEST_P(SRIMessageSignatureEnforcementTest, NoHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  auto head = ResponseHead("", "", "");
  auto result = MaybeBlockResponseForSRIMessageSignature(*head);
  EXPECT_FALSE(result.has_value());
}

TEST_P(SRIMessageSignatureEnforcementTest, ValidHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  auto head = ResponseHead(kValidDigestHeader, kValidSignatureHeader,
                           kValidSignatureInputHeader);
  auto result = MaybeBlockResponseForSRIMessageSignature(*head);
  EXPECT_FALSE(result.has_value());
}
TEST_P(SRIMessageSignatureEnforcementTest, MismatchedHeaders) {
  bool feature_flag_enabled = GetParam();

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      features::kSRIMessageSignatureEnforcement, feature_flag_enabled);

  const char* wrong_key = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const char* wrong_signature =
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  auto head = ResponseHead(kValidDigestHeader,
                           SignatureHeader("bad-signature", wrong_signature),
                           SignatureInputHeader("bad-signature", wrong_key));
  auto result = MaybeBlockResponseForSRIMessageSignature(*head);
  if (feature_flag_enabled) {
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch,
              result.value());
  } else {
    EXPECT_FALSE(result.has_value());
  }
}

}  // namespace network
