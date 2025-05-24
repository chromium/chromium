// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/two_qwac.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Builds a header that has the minimal required set of parameters
base::DictValue MinimalBindingHeader() {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  base::Value::Dict header =
      base::Value::Dict()
          .Set("alg", "test alg")
          .Set("cty", "TLS-Certificate-Binding-v1")
          .Set("x5c", base::Value::List()
                          // These are base64 encoded, not base64url encoded
                          .Append(base::Base64Encode(leaf->GetDER()))
                          .Append(base::Base64Encode(root->GetDER())))
          .Set("sigD",
               base::Value::Dict()
                   .Set("mId", "http://uri.etsi.org/19182/ObjectIdByURIHash")
                   .Set("pars", base::Value::List().Append("").Append(""))
                   .Set("hashM", "S256")
                   // These are hashes of the certs that this
                   // TlsCertificateBinding binds, not hashes of the certs in
                   // the x5c cert chain.
                   .Set("hashV", base::Value::List()
                                     .Append("fakehash1A")
                                     .Append("fakehash2A")));
  return header;
}

// Creates a TLS Certificate Binding from the provided header. This test helper
// leaves the signature empty.
std::string CreateTwoQwacCertBinding(const base::DictValue& header) {
  std::string header_string;
  EXPECT_TRUE(JSONStringValueSerializer(&header_string).Serialize(header));
  // Create the JWS from the header.
  std::string jws;
  base::Base64UrlEncode(header_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &jws);
  // Add empty payload and signature to JWS.
  jws += "..";
  return jws;
}

TEST(ParseTlsCertificateBinding, MinimalValidBinding) {
  // TODO(crbug.com/392929826): Once we start validating signatures, these
  // tests will probably need to be updated to have real algorithms,
  // certificates, and signatures. (This is assuming that some basic checks are
  // added to the parsing code, e.g. that we can parse certs into a
  // net::X509Certificate and check that the "alg" matches the leaf cert.)

  // Build a header that has the minimally required set of parameters
  base::DictValue header = MinimalBindingHeader();
  std::string jws = CreateTwoQwacCertBinding(header);
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
}

TEST(ParseTlsCertificateBinding, MaximalValidBinding) {
  // Create a header that has all allowed fields
  base::DictValue header = MinimalBindingHeader();
  header.Set("kid", base::Value::Dict()
                        .Set("random key", "random value")
                        .Set("kids can have", "whatever they want"));
  header.Set("x5t#S256", "base64urlhashA");
  header.Set("iat", 12345);
  header.Set("exp", 67.89);
  header.Set("crit", base::ListValue().Append("sigD"));

  header.FindDict("sigD")->Set(
      "ctys",
      base::Value::List().Append("content-type1").Append("content-type2"));

  std::string jws = CreateTwoQwacCertBinding(header);
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
}

// Test failure when the JWS header isn't a JSON object.
TEST(ParseTlsCertificateBinding, JwsHeaderNotObject) {
  std::string header = "[]";
  std::string jws;
  base::Base64UrlEncode(header, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &jws);
  jws += "..";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS header isn't JSON.
TEST(ParseTlsCertificateBinding, JwsHeaderNotJson) {
  std::string header = "AAA";
  std::string jws;
  base::Base64UrlEncode(header, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &jws);
  jws += "..";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS header isn't valid base64url.
TEST(ParseTlsCertificateBinding, JwsHeaderNotBase64) {
  // the header is encoded as "A", which is too short to be base64url.
  std::string jws = "A..";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS payload is non-empty.
TEST(ParseTlsCertificateBinding, JwsPayloadNonEmpty) {
  std::string header_string;
  EXPECT_TRUE(JSONStringValueSerializer(&header_string)
                  .Serialize(MinimalBindingHeader()));
  std::string header_b64;
  base::Base64UrlEncode(header_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &header_b64);
  // Make a JWS consisting of a valid header, a payload (base64url-encoded as
  // "AAAA") and an empty signature.
  std::string jws = header_b64 + ".AAAA.";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS signature is not valid base64url.
TEST(ParseTlsCertificateBinding, JwsSignatureNotBase64) {
  std::string header_string;
  EXPECT_TRUE(JSONStringValueSerializer(&header_string)
                  .Serialize(MinimalBindingHeader()));
  std::string header_b64;
  base::Base64UrlEncode(header_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &header_b64);
  std::string jws = header_b64 + "..A";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS consists of 2 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas2Components) {
  std::string header_string;
  EXPECT_TRUE(JSONStringValueSerializer(&header_string)
                  .Serialize(MinimalBindingHeader()));
  std::string header_b64;
  base::Base64UrlEncode(header_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &header_b64);
  std::string jws = header_b64 + ".AAAA";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS consists of 4 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas4Components) {
  std::string header_string;
  EXPECT_TRUE(JSONStringValueSerializer(&header_string)
                  .Serialize(MinimalBindingHeader()));
  std::string header_b64;
  base::Base64UrlEncode(header_string,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &header_b64);
  std::string jws = header_b64 + "..AAAA.AAAA";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

TEST(ParseTlsCertificateBinding, InvalidFields) {
  const struct {
    std::string header_key;
    base::Value value;
  } kTests[] = {
      {
          // "alg" expects a string
          "alg",
          base::Value(1),
      },
      {
          // "alg" expects a non-empty string
          "alg",
          base::Value(""),
      },
      {
          // "cty" expects a string
          "cty",
          base::Value(1),
      },
      {
          // "cty" expects a specific value for its string
          "cty",
          base::Value("TLS-Certificate-Binding-v2"),
      },
      {
          // "x5t#S256" expects a string
          "x5t#S256",
          base::Value(1),
      },
      {
          // "x5c" expects a list
          "x5c",
          base::Value("wrong type"),
      },
      {
          // "x5c" expects strings in its list
          "x5c",
          base::Value(base::ListValue().Append(1)),
      },
      {
          // "x5c" expects base64 strings in its list. Test with a base64url
          // (but not regular base64) string.
          "x5c",
          base::Value(base::ListValue().Append("M-_A")),
      },
      {
          // "x5c" expects the base64 strings in its list to be valid X.509
          // certificates. This string is valid base64, but is a (very)
          // truncated X.509 certificate.
          "x5c",
          base::Value(base::ListValue().Append("MIID")),
      },
      {
          // "iat" expects an int (when used for 2-QWACs). "iat" more generally
          // (according to RFC 7519) can be a double, but we don't allow that,
          // so explicitly check that doubles are rejected.
          "iat",
          base::Value(1.0),
      },
      {
          // "exp" expects a numeric value
          "exp",
          base::Value("wrong type"),
      },
      {
          // "crit", if present, can only contain "sigD"
          "crit",
          base::Value(base::ListValue().Append("sigD").Append("x5c")),
      },
      {
          // "crit" expects a list
          "crit",
          base::Value("wrong type"),
      },
      {
          // "sigD" expects an object
          "sigD",
          base::Value(base::ListValue()),
      },
      {
          // The 2-QWAC TLS Certificate Binding JAdES profile only allows
          // specific fields in the JWS header, and "x5u" is not one of them.
          "x5u",
          base::Value("X.509 URL"),
      },
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header_key);
    base::DictValue header = MinimalBindingHeader();
    header.Set(test.header_key, test.value.Clone());
    std::string jws = CreateTwoQwacCertBinding(header);
    auto cert_binding = TwoQwacCertBinding::Parse(jws);
    ASSERT_FALSE(cert_binding.has_value());
  }
}

TEST(ParseTlsCertificateBinding, SigDHeaderParam) {
  const struct {
    std::string name;
    base::RepeatingCallback<void(base::DictValue*)> header_func;
    bool valid;
  } kTests[] = {
      {
          "wrong mId",
          base::BindRepeating([](base::DictValue* sig_d) {
            sig_d->Set("mId", "http://uri.etsi.org/19182/ObjectIdByURI");
          }),
          false,
      },
      {
          "wrong mId type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("mId", 1); }),
          false,
      },
      {
          "wrong pars type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("pars", 1); }),
          false,
      },
      {
          // This repeats the default value used in MinimalBindingHeader() in
          // other tests, but is here for completeness.
          "SHA-256 supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "S256"); }),
          true,
      },
      {
          // TODO(crbug.com/392929826): Support SHA-384.
          "SHA-384 not supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "S384"); }),
          false,
      },
      {
          "SHA-512 supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "S512"); }),
          true,
      },
      {
          "Other hashM values not supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "SHA1"); }),
          false,
      },
      {
          "wrong hashM type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", 1); }),
          false,
      },
      {
          "wrong type in pars list",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "pars" and "hashV" must have the same length.
            sig_d->Set("pars", base::ListValue().Append(1));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
          }),
          false,
      },
      {
          "disallowed base64 padding in hashV",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            // hashV list elements are base64url encoded with no padding
            sig_d->Set("hashV", base::ListValue().Append("fakehashAA=="));
          }),
          false,
      },
      {
          "bad base64url encoding in hashV",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            // a base64url input (with no padding) is malformed if its length
            // mod 4 is 1.
            sig_d->Set("hashV", base::ListValue().Append("fakehash1"));
          }),
          false,
      },
      {
          "wrong type in hashV list",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV", base::ListValue().Append(1));
          }),
          false,
      },
      {
          "wrong hashV type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashV", 1); }),
          false,
      },
      {
          "correct ctys type",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
            sig_d->Set("ctys", base::ListValue().Append("content type"));
          }),
          true,
      },
      {
          "wrong ctys type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("ctys", "wrong type"); }),
          false,
      },
      {
          "wrong type inside ctys list",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
            sig_d->Set("ctys", base::ListValue().Append(1));
          }),
          false,
      },
      {
          "pars length mismatch",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL").Append("URL 2"));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
            sig_d->Set("ctys", base::ListValue().Append("content type"));
          }),
          false,
      },
      {
          "hashV length mismatch",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV",
                       base::ListValue().Append("fakehash").Append("hashfake"));
            sig_d->Set("ctys", base::ListValue().Append("content type"));
          }),
          false,
      },
      {
          "ctys length mismatch",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
            sig_d->Set("ctys", base::ListValue()
                                   .Append("content type")
                                   .Append("content type"));
          }),
          false,
      },
      {
          "unknown member in sigD",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("spURI", "URL"); }),
          false,
      },
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);
    base::DictValue header = MinimalBindingHeader();
    test.header_func.Run(header.FindDict("sigD"));
    std::string jws = CreateTwoQwacCertBinding(header);
    auto cert_binding = TwoQwacCertBinding::Parse(jws);
    EXPECT_EQ(cert_binding.has_value(), test.valid);
  }
}

}  // namespace
}  // namespace net
