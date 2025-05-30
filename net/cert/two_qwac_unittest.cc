// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/two_qwac.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {
namespace {

class TwoQwacCertBindingBuilder {
 public:
  TwoQwacCertBindingBuilder()
      : cert_chain_(net::CertBuilder::CreateSimpleChain(2)) {
    GenerateKeyForSigAlg();
    // set bound_certs_ to two bogus values
    bound_certs_ = {"one", "two"};
  }

  void SetJwsSigAlg(JwsSigAlg sig_alg) {
    sig_alg_ = sig_alg;
    GenerateKeyForSigAlg();
  }

  void SetHashAlg(crypto::hash::HashKind hash_alg) {
    hash_alg_ = hash_alg;
    Invalidate();
  }

  void SetBoundCerts(std::vector<std::string> bound_certs) {
    bound_certs_ = bound_certs;
    Invalidate();
  }

  // Set values to override in the JWS header.
  void SetHeaderOverrides(base::DictValue header_overrides) {
    Invalidate();
    header_overrides_ = std::move(header_overrides);
  }

  // Returns a pointer to the leaf net::CertBuilder.
  net::CertBuilder* GetLeafBuilder() {
    Invalidate();
    return cert_chain_[0].get();
  }

  std::string GetJWS() { return GetHeader() + ".." + GetSignature(); }

  const std::string& GetHeader() {
    if (!header_b64_.has_value()) {
      GenerateHeader();
    }
    return *header_b64_;
  }

  const std::string& GetSignature() {
    if (!signature_b64_.has_value()) {
      GenerateSignature();
    }
    return *signature_b64_;
  }

 private:
  void GenerateKeyForSigAlg() {
    switch (sig_alg_) {
      case JwsSigAlg::kRsaPkcs1Sha256:
      case JwsSigAlg::kRsaPssSha256:
        cert_chain_[0]->GenerateRSAKey();
        break;
      case JwsSigAlg::kEcdsaP256Sha256:
        cert_chain_[0]->GenerateECKey();
    }
    Invalidate();
  }

  std::string SigAlg() const {
    switch (sig_alg_) {
      case JwsSigAlg::kRsaPkcs1Sha256:
        return "RS256";
      case JwsSigAlg::kRsaPssSha256:
        return "PS256";
      case JwsSigAlg::kEcdsaP256Sha256:
        return "ES256";
    }
    return "";
  }

  std::string HashAlg() {
    switch (hash_alg_) {
      case crypto::hash::kSha256:
        return "S256";
      case crypto::hash::kSha384:
        return "S384";
      case crypto::hash::kSha512:
        return "S512";
      default:
        return "";
    }
  }

  base::ListValue GenerateX5cHeaderValue() {
    base::ListValue x5c_list;
    for (const auto& cert : cert_chain_) {
      x5c_list.Append(base::Base64Encode(cert->GetDER()));
    }
    return x5c_list;
  }

  base::DictValue GenerateSigDHeaderValue() {
    base::DictValue sig_d =
        base::Value::Dict()
            .Set("mId", "http://uri.etsi.org/19182/ObjectIdByURIHash")
            .Set("hashM", HashAlg());
    base::ListValue pars;
    base::ListValue hash_v;
    for (const auto& bound_cert : bound_certs_) {
      // ETSI TS 119 182-1 clause 5.2.8.1: Each element of the "hashV" array
      // shall contain the base64url-encoded digest value of the
      // base64url-encoded data object.
      std::string cert_b64;
      base::Base64UrlEncode(
          bound_cert, base::Base64UrlEncodePolicy::OMIT_PADDING, &cert_b64);
      std::vector<uint8_t> cert_hash(
          crypto::hash::DigestSizeForHashKind(hash_alg_));
      crypto::hash::Hash(hash_alg_, cert_b64, cert_hash);
      std::string hash_b64;
      base::Base64UrlEncode(
          cert_hash, base::Base64UrlEncodePolicy::OMIT_PADDING, &hash_b64);
      hash_v.Append(hash_b64);
      pars.Append("");
    }
    sig_d.Set("pars", std::move(pars));
    sig_d.Set("hashV", std::move(hash_v));
    return sig_d;
  }

  void GenerateHeader() {
    // Build the minimal JWS header needed for a 2-QWAC TLS certificate binding.
    base::DictValue header = base::DictValue()
                                 .Set("alg", SigAlg())
                                 .Set("cty", "TLS-Certificate-Binding-v1")
                                 .Set("x5c", GenerateX5cHeaderValue())
                                 .Set("sigD", GenerateSigDHeaderValue());
    // Add/override values in the header
    header.Merge(header_overrides_.Clone());

    std::string header_string;
    ASSERT_TRUE(JSONStringValueSerializer(&header_string).Serialize(header));
    header_b64_ = std::string();
    base::Base64UrlEncode(header_string,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &*header_b64_);
  }

  void GenerateSignature() {
    // All JWS signature algorithms that we support use SHA-256 as their digest.
    const EVP_MD* digest = EVP_sha256();
    bssl::ScopedEVP_MD_CTX ctx;
    EVP_PKEY_CTX* pkey_ctx;
    EVP_PKEY* key = cert_chain_[0]->GetKey();
    ASSERT_TRUE(
        EVP_DigestSignInit(ctx.get(), &pkey_ctx, EVP_sha256(), nullptr, key));
    if (sig_alg_ == JwsSigAlg::kRsaPssSha256) {
      ASSERT_TRUE(
          EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING));
      ASSERT_TRUE(EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, digest));
      ASSERT_TRUE(EVP_PKEY_CTX_set_rsa_pss_saltlen(
          pkey_ctx, -1 /* match digest and salt length */));
    }

    // The JWS signing input is the the (base64url-encoded) header and payload
    // concatenated and separated by a '.'. For a 2-QWAC cert binding, the
    // payload is always empty.
    const std::string& header = GetHeader();
    const std::array<uint8_t, 1> separator = {'.'};
    ASSERT_TRUE(EVP_DigestSignUpdate(ctx.get(), header.data(), header.size()));
    ASSERT_TRUE(
        EVP_DigestSignUpdate(ctx.get(), separator.data(), separator.size()));
    size_t len = 0;
    std::vector<uint8_t> sig;
    ASSERT_TRUE(EVP_DigestSignFinal(ctx.get(), nullptr, &len));
    sig.resize(len);
    ASSERT_TRUE(EVP_DigestSignFinal(ctx.get(), sig.data(), &len));
    sig.resize(len);
    signature_b64_ = std::string();
    base::Base64UrlEncode(sig, base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &*signature_b64_);
  }

  void Invalidate() {
    header_b64_.reset();
    signature_b64_.reset();
  }

  std::vector<std::unique_ptr<net::CertBuilder>> cert_chain_;
  std::vector<std::string> bound_certs_;
  base::DictValue header_overrides_;
  JwsSigAlg sig_alg_ = JwsSigAlg::kEcdsaP256Sha256;
  crypto::hash::HashKind hash_alg_ = crypto::hash::kSha256;
  // The header and signature are lazily built, and if any inputs to the builder
  // are possibly modified, then they are cleared.
  std::optional<std::string> header_b64_;
  std::optional<std::string> signature_b64_;
};

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
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  // Make a JWS consisting of a valid header, a payload (base64url-encoded as
  // "AAAA") and an empty signature.
  std::string jws = header_b64 + ".AAAA.";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS signature is not valid base64url.
TEST(ParseTlsCertificateBinding, JwsSignatureNotBase64) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  std::string jws = header_b64 + "..A";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS consists of 2 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas2Components) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
  std::string jws = header_b64 + ".AAAA";
  EXPECT_FALSE(TwoQwacCertBinding::Parse(jws).has_value());
}

// Test failure when the JWS consists of 4 components instead of 3.
TEST(ParseTlsCertificateBinding, JwsHas4Components) {
  std::string header_b64 = TwoQwacCertBindingBuilder().GetHeader();
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
          // "alg" expects a supported signature algorithm from the IANA
          // registry. "none" is in the registry but we will never support it.
          "alg",
          base::Value("none"),
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
    TwoQwacCertBindingBuilder binding_builder;
    binding_builder.SetHeaderOverrides(
        base::DictValue().Set(test.header_key, test.value.Clone()));
    std::string jws = binding_builder.GetJWS();
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
    base::DictValue sig_d;
    test.header_func.Run(&sig_d);
    TwoQwacCertBindingBuilder binding_builder;
    binding_builder.SetHeaderOverrides(
        base::DictValue().Set("sigD", std::move(sig_d)));
    std::string jws = binding_builder.GetJWS();
    auto cert_binding = TwoQwacCertBinding::Parse(jws);
    EXPECT_EQ(cert_binding.has_value(), test.valid);
  }
}

TEST(VerifyTwoQwacCertBinding, ValidSignatureRS256) {
  TwoQwacCertBindingBuilder binding_builder;
  // Use RSA-PKCS1v1.5 for the TLS Certificate Binding.
  binding_builder.SetJwsSigAlg(JwsSigAlg::kRsaPkcs1Sha256);
  std::string jws = binding_builder.GetJWS();

  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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

  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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

  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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

  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  // Check that the JWS header has "alg": "ES256"
  EXPECT_EQ(cert_binding->header().sig_alg, JwsSigAlg::kEcdsaP256Sha256);
  // Since the key uses the wrong curve, the signature verification should fail.
  EXPECT_FALSE(cert_binding->VerifySignature());
}

TEST(VerifyTwoQwacCertBinding, InvalidSignature) {
  TwoQwacCertBindingBuilder binding_builder;
  const auto& header = binding_builder.GetHeader();
  std::string signature = binding_builder.GetSignature();

  // Build the JWS from the header and signature and confirm the signature is
  // valid.
  std::string jws = header + ".." + signature;
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());
  EXPECT_TRUE(cert_binding->VerifySignature());

  // Mess with the base64url-encoded signature to make it invalid.
  if (signature[0] != 'A') {
    signature[0] = 'A';
  } else {
    signature[0] = 'B';
  }

  // rebuild the JWS with the invalid signature, and check that the signature
  // is invalid.
  std::string jws_bad_sig = header + ".." + signature;
  std::optional<TwoQwacCertBinding> cert_binding_bad_sig =
      TwoQwacCertBinding::Parse(jws_bad_sig);
  ASSERT_TRUE(cert_binding_bad_sig.has_value());
  EXPECT_FALSE(cert_binding_bad_sig->VerifySignature());
}

TEST(TwoQwacCertBinding, BoundCertPresent) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  std::string bound_cert = leaf->GetDER();
  TwoQwacCertBindingBuilder binding_builder;
  binding_builder.SetBoundCerts({bound_cert});
  std::string jws = binding_builder.GetJWS();
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
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
  std::optional<TwoQwacCertBinding> cert_binding =
      TwoQwacCertBinding::Parse(jws);
  ASSERT_TRUE(cert_binding.has_value());

  EXPECT_TRUE(cert_binding->BindsTlsCert(base::as_byte_span(bound_cert)));
}

}  // namespace
}  // namespace net
