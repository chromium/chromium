// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/two_qwac.h"

#include <stdint.h>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "net/test/cert_builder.h"
#include "net/test/two_qwac_cert_binding_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {
namespace {

TEST(ParseTlsCertificateBinding, MinimalValidBinding) {
  // Build a header that has the minimally required set of parameters
  TwoQwacCertBindingBuilder binding_builder;
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
}

TEST(ParseTlsCertificateBinding, MaximalValidBinding) {
  TwoQwacCertBindingBuilder binding_builder;
  // Set all of the optional fields in the header.
  binding_builder.SetHeaderOverrides(
      base::DictValue()
          .Set("kid", base::Value::Dict()
                          .Set("random key", "random value")
                          .Set("kids can have", "whatever they want"))
          .Set("x5t#S256", "base64urlhashA")
          .Set("iat", 12345)
          .Set("exp", 67.89)
          .Set("crit", base::ListValue().Append("sigD"))
          .Set("sigD",
               base::DictValue().Set("ctys", base::Value::List()
                                                 .Append("content-type1")
                                                 .Append("content-type2"))));
  std::string jws = binding_builder.GetJWS();

  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
}

TEST(ParseTlsCertificateBinding, RS256ValidSigAlg) {
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetHeaderOverrides(base::DictValue().Set("alg", "RS256"));
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  ASSERT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kRsaPkcs1Sha256);
}

TEST(ParseTlsCertificateBinding, PS256ValidSigAlg) {
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetHeaderOverrides(base::DictValue().Set("alg", "PS256"));
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  ASSERT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kRsaPssSha256);
}

TEST(ParseTlsCertificateBinding, InvalidSigAlg) {
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetHeaderOverrides(base::DictValue().Set("alg", "RSA1_5"));
  std::string jws = binding_builder.GetJWS();
  EXPECT_FALSE(jws.empty());
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "header parsing error: unsupported alg");
}

// Test failure when the JWS header isn't a JSON object.
TEST(ParseTlsCertificateBinding, JwsHeaderNotObject) {
  std::string header = "[]";
  std::string jws;
  base::Base64UrlEncode(header, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &jws);
  jws += "..";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "header parsing error: JSON not a dict");
}

// Test failure when the JWS header isn't JSON.
TEST(ParseTlsCertificateBinding, JwsHeaderNotJson) {
  std::string header = "AAA";
  std::string jws;
  base::Base64UrlEncode(header, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &jws);
  jws += "..";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "header parsing error: JSON parsing error");
}

// Test failure when the JWS header isn't valid base64url.
TEST(ParseTlsCertificateBinding, JwsHeaderNotBase64) {
  // the header is encoded as "A", which is too short to be base64url.
  std::string jws = "A..";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "base64 decoding header error");
}

// Test failure when the JWS payload is non-empty.
TEST(ParseTlsCertificateBinding, JwsPayloadNonEmpty) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  // Make a JWS consisting of a valid header, a payload (base64url-encoded as
  // "AAAA") and an empty signature.
  std::string jws = header_b64 + ".AAAA.";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "payload is non-empty");
}

// Test failure when the JWS signature is not valid base64url.
TEST(ParseTlsCertificateBinding, JwsSignatureNotBase64) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  std::string jws = header_b64 + "..A";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "base64 decoding signature error");
}

// Test failure when the JWS consists of 2 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas2Components) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  std::string jws = header_b64 + ".AAAA";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "wrong number of components");
}

// Test failure when the JWS consists of 4 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas4Components) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  std::string jws = header_b64 + "..AAAA.AAAA";
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_FALSE(cert_binding.has_value());
  EXPECT_EQ(cert_binding.error(), "wrong number of components");
}

TEST(ParseTlsCertificateBinding, InvalidFields) {
  const struct {
    std::string header_key;
    base::Value value;
    std::string expected_error;
  } kTests[] = {
      {
          // "alg" expects a string
          "alg",
          base::Value(1),
          "alg missing or not a string",
      },
      {
          // "alg" expects a supported signature algorithm from the IANA
          // registry. "none" is in the registry but we will never support it.
          "alg",
          base::Value("none"),
          "unsupported alg",
      },
      {
          // "cty" expects a string
          "cty",
          base::Value(1),
          "cty missing or not a string",
      },
      {
          // "cty" expects a specific value for its string
          "cty",
          base::Value("TLS-Certificate-Binding-v2"),
          "unsupported cty",
      },
      {
          // "x5t#S256" expects a string
          "x5t#S256",
          base::Value(1),
          // The "x5t#S256" is ignored if it is a string, otherwise it hits the
          // unexpected members failure case.
          "header has unexpected members",
      },
      {
          // "x5c" expects a list
          "x5c",
          base::Value("wrong type"),
          "x5c missing or not a list",
      },
      {
          // "x5c" expects strings in its list
          "x5c",
          base::Value(base::ListValue().Append(1)),
          "x5c element not a string",
      },
      {
          // "x5c" expects base64 strings in its list. Test with a base64url
          // (but not regular base64) string.
          "x5c",
          base::Value(base::ListValue().Append("M-_A")),
          "x5c element base64 decode error",
      },
      {
          // "x5c" expects the base64 strings in its list to be valid X.509
          // certificates. This string is valid base64, but is a (very)
          // truncated X.509 certificate.
          "x5c",
          base::Value(base::ListValue().Append("MIID")),
          "x5c cert parsing error",
      },
      {
          // "iat" expects an int (when used for 2-QWACs). "iat" more generally
          // (according to RFC 7519) can be a double, but we don't allow that,
          // so explicitly check that doubles are rejected.
          "iat",
          base::Value(1.0),
          // The "iat" is ignored if it is an integer, otherwise it hits the
          // unexpected members failure case.
          "header has unexpected members",
      },
      {
          // "exp" expects a numeric value
          "exp",
          base::Value("wrong type"),
          // The "exp" is ignored if it is an integer, otherwise it hits the
          // unexpected members failure case.
          "header has unexpected members",
      },
      {
          // "crit", if present, can only contain "sigD"
          "crit",
          base::Value(base::ListValue().Append("sigD").Append("x5c")),
          "crit contains non sigD element(s)",
      },
      {
          // "crit" expects a list
          "crit",
          base::Value("wrong type"),
          "crit not a list",
      },
      {
          // "sigD" expects an object
          "sigD",
          base::Value(base::ListValue()),
          "sigD missing or not a dict",
      },
      {
          // The 2-QWAC TLS Certificate Binding JAdES profile only allows
          // specific fields in the JWS header, and "x5u" is not one of them.
          "x5u",
          base::Value("X.509 URL"),
          "header has unexpected members",
      },
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header_key);
    TwoQwacCertBindingBuilder binding_builder;
    binding_builder.SetHeaderOverrides(
        base::DictValue().Set(test.header_key, test.value.Clone()));
    std::string jws = binding_builder.GetJWS();
    auto cert_binding = TwoQwacCertBinding::Parse(jws);
    ASSERT_FALSE(cert_binding.has_value());
    EXPECT_EQ(cert_binding.error(),
              "header parsing error: " + test.expected_error);
  }
}

TEST(ParseTlsCertificateBinding, SigDHeaderParam) {
  const struct {
    std::string name;
    base::RepeatingCallback<void(base::DictValue*)> header_func;
    bool valid;
    std::string expected_error;  // Ignored if valid.
  } kTests[] = {
      {
          "wrong mId",
          base::BindRepeating([](base::DictValue* sig_d) {
            sig_d->Set("mId", "http://uri.etsi.org/19182/ObjectIdByURI");
          }),
          false,
          "sigD: invalid mId",
      },
      {
          "wrong mId type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("mId", 1); }),
          false,
          "sigD: mId missing or not a string",
      },
      {
          "wrong pars type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("pars", 1); }),
          false,
          "sigD: pars missing or not a list",
      },
      {
          "SHA-256 supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "S256"); }),
          true,
      },
      {
          "SHA-384 supported",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", "S384"); }),
          true,
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
          "sigD: unsupported hashM",
      },
      {
          "wrong hashM type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashM", 1); }),
          false,
          "sigD: hashM missing or not a string",
      },
      {
          "wrong type in pars list",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "pars" and "hashV" must have the same length.
            sig_d->Set("pars", base::ListValue().Append(1));
            sig_d->Set("hashV", base::ListValue().Append("fakehash"));
          }),
          false,
          "sigD: pars element not a string",
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
          "sigD: hashV element base64 decode error",
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
          "sigD: hashV element base64 decode error",
      },
      {
          "wrong type in hashV list",
          base::BindRepeating([](base::DictValue* sig_d) {
            // "ctys" must have the same length as "pars" and "hashV".
            sig_d->Set("pars", base::ListValue().Append("URL"));
            sig_d->Set("hashV", base::ListValue().Append(1));
          }),
          false,
          "sigD: hashV element not a string",
      },
      {
          "wrong hashV type",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("hashV", 1); }),
          false,
          "sigD: hashV missing or not a list",
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
          "sigD: ctys not a list",
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
          "sigD: ctys element not a string",
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
          "sigD: hashV count doesn't match pars count",
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
          "sigD: hashV count doesn't match pars count",
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
          "sigD: ctys count doesn't match pars count",
      },
      {
          "unknown member in sigD",
          base::BindRepeating(
              [](base::DictValue* sig_d) { sig_d->Set("spURI", "URL"); }),
          false,
          "sigD has unexpected members",
      },
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);
    base::DictValue sig_d;
    test.header_func.Run(&sig_d);
    TwoQwacCertBindingBuilder binding_builder;
    binding_builder.SetHeaderOverrides(
        base::DictValue().Set("sigD", std::move(sig_d)));
    std::string jws = binding_builder.GetJWS();
    auto cert_binding = TwoQwacCertBinding::Parse(jws);
    EXPECT_EQ(cert_binding.has_value(), test.valid);
    if (!cert_binding.has_value()) {
      EXPECT_EQ(cert_binding.error(),
                "header parsing error: " + test.expected_error);
    }
  }
}

TEST(VerifyTwoQwacCertBinding, ValidSignatureRS256) {
  TwoQwacCertBindingBuilder binding_builder;
  // Use RSA-PKCS1v1.5 for the TLS Certificate Binding.
  binding_builder.SetJwsSigAlg(JwsSigAlg::kRsaPkcs1Sha256);
  std::string jws = binding_builder.GetJWS();

  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  // Check that the JWS header has "alg": "RS256"
  EXPECT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kRsaPkcs1Sha256);
  EXPECT_TRUE(cert_binding->VerifySignature());
}

TEST(VerifyTwoQwacCertBinding, ValidSignaturePS256) {
  TwoQwacCertBindingBuilder binding_builder;
  // Use RSA-PSS for the TLS Certificate Binding.
  binding_builder.SetJwsSigAlg(JwsSigAlg::kRsaPssSha256);
  std::string jws = binding_builder.GetJWS();

  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  // Check that the JWS header has "alg": "PS256"
  EXPECT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kRsaPssSha256);
  EXPECT_TRUE(cert_binding->VerifySignature());
}

TEST(VerifyTwoQwacCertBinding, ValidSignatureES256) {
  TwoQwacCertBindingBuilder binding_builder;
  // Use ECDSA for the TLS Certificate Binding.
  binding_builder.SetJwsSigAlg(JwsSigAlg::kEcdsaP256Sha256);
  std::string jws = binding_builder.GetJWS();

  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  // Check that the JWS header has "alg": "ES256"
  EXPECT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kEcdsaP256Sha256);
  EXPECT_TRUE(cert_binding->VerifySignature());
}

TEST(VerifyTwoQwacCertBinding, InvalidEcdsaCurve) {
  TwoQwacCertBindingBuilder binding_builder;
  // Set "ES256" as the JWS signature algorithm.
  binding_builder.SetJwsSigAlg(JwsSigAlg::kEcdsaP256Sha256);
  // Set the leaf cert to use a P-384 key.
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp384r1));
  ASSERT_TRUE(EC_KEY_generate_key(ec_key.get()));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  ASSERT_TRUE(EVP_PKEY_assign_EC_KEY(pkey.get(), ec_key.release()));
  binding_builder.GetLeafBuilder()->SetKey(std::move(pkey));

  std::string jws = binding_builder.GetJWS();

  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  // Check that the JWS header has "alg": "ES256"
  EXPECT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kEcdsaP256Sha256);
  // Since the key uses the wrong curve, the signature verification should fail.
  EXPECT_FALSE(cert_binding->VerifySignature());
}

TEST(VerifyTwoQwacCertBinding, InvalidSignature) {
  TwoQwacCertBindingBuilder binding_builder;

  // Build a JWS with an invalid signature, and check that the signature
  // is invalid.
  auto cert_binding_bad_sig =
      TwoQwacCertBinding::Parse(binding_builder.GetJWSWithInvalidSignature());
  ASSERT_TRUE(cert_binding_bad_sig.has_value());
  EXPECT_FALSE(cert_binding_bad_sig->VerifySignature());
}

TEST(TwoQwacCertBinding, BoundCertPresent) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert = leaf->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert});
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert)));
}

TEST(TwoQwacCertBinding, MultipleBoundCerts) {
  auto [leaf1, root1] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert1 = leaf1->GetDER();
  auto [leaf2, root2] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert2 = leaf2->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert1, bound_cert2});
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert1)));
  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert2)));
}

TEST(TwoQwacCertBinding, UnboundCertNotFound) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert = leaf->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert});
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  auto [leaf2, root2] = net::CertBuilder::CreateSimpleChain2();
  std::string unbound_cert = leaf2->GetDER();
  EXPECT_FALSE(cert_binding->BindsTlsCert(base::as_byte_span(unbound_cert)));
}

TEST(TwoQwacCertBinding, BoundCertPresentSha384) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert = leaf->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert});
  binding_builder.SetHashAlg(crypto::hash::kSha384);
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert)));
}

TEST(TwoQwacCertBinding, BoundCertPresentSha512) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert = leaf->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert});
  binding_builder.SetHashAlg(crypto::hash::kSha512);
  std::string jws = binding_builder.GetJWS();
  auto cert_binding = TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert)));
}

}  // namespace
}  // namespace net
