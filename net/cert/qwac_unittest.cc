// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/qwac.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "net/test/cert_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {
namespace {

TEST(ParseQcStatements, InvalidSequence) {
  EXPECT_EQ(std::nullopt, ParseQcStatements(bssl::der::Input("invalid")));
}

TEST(ParseQcStatements, EmptySequence) {
  constexpr uint8_t kEmptySequence[] = {0x30, 0x0};
  // An empty QCStatements sequence doesn't really make sense, but RFC 3739
  // does not specify that the sequence must be non-empty, so we allow it.
  auto r = ParseQcStatements(bssl::der::Input(kEmptySequence));
  ASSERT_TRUE(r.has_value());
  EXPECT_THAT(r.value(), testing::IsEmpty());
}

TEST(ParseQcStatements, InvalidStatementSequence) {
  // SEQUENCE { OBJECT_IDENTIFIER { 1.2.3 } }
  constexpr uint8_t kInvalid[] = {0x30, 0x04, 0x06, 0x02, 0x2a, 0x03};
  EXPECT_EQ(std::nullopt, ParseQcStatements(bssl::der::Input(kInvalid)));
}

TEST(ParseQcStatements, InvalidStatementOid) {
  // SEQUENCE { SEQUENCE { SEQUENCE { OBJECT_IDENTIFIER { 1.2.4 } } } }
  constexpr uint8_t kInvalid[] = {0x30, 0x08, 0x30, 0x06, 0x30,
                                  0x04, 0x06, 0x02, 0x2a, 0x04};
  EXPECT_EQ(std::nullopt, ParseQcStatements(bssl::der::Input(kInvalid)));
}

TEST(ParseQcStatements, MultipleStatementsSomeWithInfo) {
  // SEQUENCE {
  //   SEQUENCE {
  //     OBJECT_IDENTIFIER { 1.3.6.1.5.5.7.11.2 }
  //     SEQUENCE {
  //       OBJECT_IDENTIFIER { 0.4.0.194121.1.2 }
  //     }
  //   }
  //   SEQUENCE {
  //     OBJECT_IDENTIFIER { 0.4.0.1862.1.1 }
  //   }
  //   SEQUENCE {
  //     OBJECT_IDENTIFIER { 0.4.0.1862.1.6 }
  //     SEQUENCE {
  //       OBJECT_IDENTIFIER { 0.4.0.1862.1.6.3 }
  //     }
  //   }
  // }
  constexpr uint8_t kQcStatementsValue[] = {
      0x30, 0x36, 0x30, 0x15, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
      0x0b, 0x02, 0x30, 0x09, 0x06, 0x07, 0x04, 0x00, 0x8b, 0xec, 0x49, 0x01,
      0x02, 0x30, 0x08, 0x06, 0x06, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x01, 0x30,
      0x13, 0x06, 0x06, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x06, 0x30, 0x09, 0x06,
      0x07, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x06, 0x03};
  auto r = ParseQcStatements(bssl::der::Input(kQcStatementsValue));
  ASSERT_TRUE(r.has_value());
  ASSERT_EQ(3u, r->size());

  constexpr uint8_t id_0[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x0b, 0x02};
  EXPECT_EQ(bssl::der::Input(id_0), r.value()[0].id);
  constexpr uint8_t info_0[] = {0x30, 0x09, 0x06, 0x07, 0x04, 0x00,
                                0x8b, 0xec, 0x49, 0x01, 0x02};
  EXPECT_EQ(bssl::der::Input(info_0), r.value()[0].info);

  EXPECT_EQ(bssl::der::Input(kEtsiQcsQcComplianceOid), r.value()[1].id);
  EXPECT_TRUE(r.value()[1].info.empty());

  EXPECT_EQ(bssl::der::Input(kEtsiQcsQcTypeOid), r.value()[2].id);
  constexpr uint8_t info_2[] = {0x30, 0x09, 0x06, 0x07, 0x04, 0x00,
                                0x8e, 0x46, 0x01, 0x06, 0x03};
  EXPECT_EQ(bssl::der::Input(info_2), r.value()[2].info);
}

TEST(HasQwacQcStatements, Empty) {
  EXPECT_EQ(QwacQcStatementsStatus::kNotQwac, HasQwacQcStatements({}));
}

TEST(ParseQcTypeInfo, InvalidSequence) {
  EXPECT_EQ(std::nullopt, ParseQcTypeInfo(bssl::der::Input("invalid")));
}

TEST(ParseQcTypeInfo, EmptySequence) {
  constexpr uint8_t kEmptySequence[] = {0x30, 0x0};
  // An empty QCTypeInfo sequence doesn't really make sense, but the spec does
  // not specify that the sequence must be non-empty, so we allow it.
  auto r = ParseQcTypeInfo(bssl::der::Input(kEmptySequence));
  ASSERT_TRUE(r.has_value());
  EXPECT_THAT(r.value(), testing::IsEmpty());
}

TEST(ParseQcTypeInfo, SingleValue) {
  // SEQUENCE { OBJECT_IDENTIFIER { 1.2.3 } }
  constexpr uint8_t kQcTypeInfo[] = {0x30, 0x04, 0x06, 0x02, 0x2a, 0x03};
  constexpr uint8_t kOid[] = {0x2a, 0x03};
  auto r = ParseQcTypeInfo(bssl::der::Input(kQcTypeInfo));
  ASSERT_TRUE(r.has_value());
  EXPECT_THAT(r.value(), testing::ElementsAre(bssl::der::Input(kOid)));
}

TEST(ParseQcTypeInfo, MultipleValues) {
  // SEQUENCE {
  //   OBJECT_IDENTIFIER { 1.2.3 }
  //   OBJECT_IDENTIFIER { 2.1.6 }
  //   OBJECT_IDENTIFIER { 1.2.4 }
  // }
  constexpr uint8_t kQcTypeInfo[] = {0x30, 0x0c, 0x06, 0x02, 0x2a, 0x03, 0x06,
                                     0x02, 0x51, 0x06, 0x06, 0x02, 0x2a, 0x04};
  constexpr uint8_t kOid1[] = {0x2a, 0x03};
  constexpr uint8_t kOid2[] = {0x51, 0x06};
  constexpr uint8_t kOid3[] = {0x2a, 0x04};
  auto r = ParseQcTypeInfo(bssl::der::Input(kQcTypeInfo));
  ASSERT_TRUE(r.has_value());
  EXPECT_THAT(r.value(), testing::ElementsAre(bssl::der::Input(kOid1),
                                              bssl::der::Input(kOid2),
                                              bssl::der::Input(kOid3)));
}

TEST(ParseQcTypeInfo, InvalidTrailingData) {
  // SEQUENCE { OBJECT_IDENTIFIER { 1.2.3 } } SEQUENCE { }
  constexpr uint8_t kInvalid[] = {0x30, 0x04, 0x06, 0x02,
                                  0x2a, 0x03, 0x30, 0x0};
  EXPECT_EQ(std::nullopt, ParseQcTypeInfo(bssl::der::Input(kInvalid)));
}

TEST(ParseQcTypeInfo, InvalidStatementOid) {
  // SEQUENCE { SEQUENCE { SEQUENCE { OBJECT_IDENTIFIER { 1.2.4 } } } }
  constexpr uint8_t kInvalid[] = {0x30, 0x08, 0x30, 0x06, 0x30,
                                  0x04, 0x06, 0x02, 0x2a, 0x04};
  EXPECT_EQ(std::nullopt, ParseQcTypeInfo(bssl::der::Input(kInvalid)));
}

// A valid QcStatement which has a id-etsi-qcs-QcCompliance statement and a
// id-etsi-qcs-QcType statement which contains id-etsi-qct-web. Is a
// QwacQcStatement.
TEST(HasQwacQcStatements, Valid) {
  std::vector<QcStatement> statements;

  statements.emplace_back(bssl::der::Input(kEtsiQcsQcComplianceOid),
                          bssl::der::Input());

  // SEQUENCE { OBJECT_IDENTIFIER { 0.4.0.1862.1.6.3 } }
  constexpr uint8_t kQctWebOidSequence[] = {0x30, 0x09, 0x06, 0x07, 0x04, 0x00,
                                            0x8e, 0x46, 0x01, 0x06, 0x03};
  statements.emplace_back(bssl::der::Input(kEtsiQcsQcTypeOid),
                          bssl::der::Input(kQctWebOidSequence));

  EXPECT_EQ(QwacQcStatementsStatus::kHasQwacStatements,
            HasQwacQcStatements(statements));
}

// A QcStatement which has a id-etsi-qcs-QcCompliance statement but does not
// have a id-etsi-qcs-QcType statement. Is not a QwacQcStatement.
TEST(HasQwacQcStatements, NoQcType) {
  std::vector<QcStatement> statements;

  statements.emplace_back(bssl::der::Input(kEtsiQcsQcComplianceOid),
                          bssl::der::Input());

  EXPECT_EQ(QwacQcStatementsStatus::kInconsistent,
            HasQwacQcStatements(statements));
}

// A QcStatement which has a id-etsi-qcs-QcCompliance statement and a
// id-etsi-qcs-QcType statement, but the QcType info does not contain
// id-etsi-qct-web. Is not a QwacQcStatement.
TEST(HasQwacQcStatements, WrongQcType) {
  std::vector<QcStatement> statements;

  statements.emplace_back(bssl::der::Input(kEtsiQcsQcComplianceOid),
                          bssl::der::Input());

  // SEQUENCE { OBJECT_IDENTIFIER { 0.4.0.1862.1.6.2 } }
  constexpr uint8_t kQctEsealOidSequence[] = {
      0x30, 0x09, 0x06, 0x07, 0x04, 0x00, 0x8e, 0x46, 0x01, 0x06, 0x02};
  statements.emplace_back(bssl::der::Input(kEtsiQcsQcTypeOid),
                          bssl::der::Input(kQctEsealOidSequence));

  EXPECT_EQ(QwacQcStatementsStatus::kInconsistent,
            HasQwacQcStatements(statements));
}

// A QcStatement which has a id-etsi-qcs-QcType statement of type
// id-etsi-qct-web but does not contain a a id-etsi-qcs-QcCompliance statement.
// Is not a QwacQcStatement.
TEST(HasQwacQcStatements, NoQcCompliance) {
  std::vector<QcStatement> statements;

  // SEQUENCE { OBJECT_IDENTIFIER { 0.4.0.1862.1.6.3 } }
  constexpr uint8_t kQctWebOidSequence[] = {0x30, 0x09, 0x06, 0x07, 0x04, 0x00,
                                            0x8e, 0x46, 0x01, 0x06, 0x03};
  statements.emplace_back(bssl::der::Input(kEtsiQcsQcTypeOid),
                          bssl::der::Input(kQctWebOidSequence));

  EXPECT_EQ(QwacQcStatementsStatus::kInconsistent,
            HasQwacQcStatements(statements));
}

TEST(Has1QwacPolicies, TestPolicyCases) {
  struct TestCase {
    QwacPoliciesStatus expected_qwacness;
    std::vector<bssl::der::Input> policies;
  } kTestCases[] = {
      // Expected QWAC cases:
      {QwacPoliciesStatus::kHasQwacPolicies,
       {bssl::der::Input(kCabfBrEvOid), bssl::der::Input(kQevcpwOid)}},
      {QwacPoliciesStatus::kHasQwacPolicies,
       {bssl::der::Input(kCabfBrIvOid), bssl::der::Input(kQncpwOid)}},
      {QwacPoliciesStatus::kHasQwacPolicies,
       {bssl::der::Input(kCabfBrOvOid), bssl::der::Input(kQncpwOid)}},
      // Mismatch between EV/non-EV policies:
      {QwacPoliciesStatus::kInconsistent,
       {bssl::der::Input(kCabfBrEvOid), bssl::der::Input(kQncpwOid)}},
      {QwacPoliciesStatus::kInconsistent,
       {bssl::der::Input(kCabfBrIvOid), bssl::der::Input(kQevcpwOid)}},
      {QwacPoliciesStatus::kInconsistent,
       {bssl::der::Input(kCabfBrOvOid), bssl::der::Input(kQevcpwOid)}},
      // Trying to use 2-QWAC policy on a 1-QWAC:
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrEvOid), bssl::der::Input(kQncpwgenOid)}},
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrIvOid), bssl::der::Input(kQncpwgenOid)}},
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrOvOid), bssl::der::Input(kQncpwgenOid)}},
      // Only has EU policies but doesn't have any CABF policy:
      {QwacPoliciesStatus::kInconsistent, {bssl::der::Input(kQevcpwOid)}},
      {QwacPoliciesStatus::kInconsistent, {bssl::der::Input(kQncpwOid)}},
      // Only has CABF policies:
      {QwacPoliciesStatus::kNotQwac, {bssl::der::Input(kCabfBrEvOid)}},
      {QwacPoliciesStatus::kNotQwac, {bssl::der::Input(kCabfBrIvOid)}},
      {QwacPoliciesStatus::kNotQwac, {bssl::der::Input(kCabfBrOvOid)}},
      // No policies:
      {QwacPoliciesStatus::kNotQwac, {}},
  };

  constexpr uint8_t kUnrelated[] = {0x01, 0x02, 0x03};
  for (const auto& [expected_qwacness, policies] : kTestCases) {
    std::set<bssl::der::Input> policy_set(policies.begin(), policies.end());
    EXPECT_EQ(expected_qwacness, Has1QwacPolicies(policy_set));

    // Adding additional unrelated policies to the policy set should not change
    // the result.
    policy_set.insert(bssl::der::Input(kUnrelated));
    EXPECT_EQ(expected_qwacness, Has1QwacPolicies(policy_set));
  }
}

TEST(Has2QwacPolicies, TestPolicyCases) {
  struct TestCase {
    QwacPoliciesStatus expected_qwacness;
    std::vector<bssl::der::Input> policies;
  } kTestCases[] = {
      // Expected QWAC cases:
      {QwacPoliciesStatus::kHasQwacPolicies, {bssl::der::Input(kQncpwgenOid)}},
      // Trying to use 1-QWAC policies on a 2-QWAC:
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrEvOid), bssl::der::Input(kQevcpwOid)}},
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrIvOid), bssl::der::Input(kQncpwOid)}},
      {QwacPoliciesStatus::kNotQwac,
       {bssl::der::Input(kCabfBrOvOid), bssl::der::Input(kQncpwOid)}},
      // No policies:
      {QwacPoliciesStatus::kNotQwac, {}},
  };

  constexpr uint8_t kUnrelated[] = {0x01, 0x02, 0x03};
  for (const auto& [expected_qwacness, policies] : kTestCases) {
    std::set<bssl::der::Input> policy_set(policies.begin(), policies.end());
    EXPECT_EQ(expected_qwacness, Has2QwacPolicies(policy_set));

    // Adding additional unrelated policies to the policy set should not change
    // the result.
    policy_set.insert(bssl::der::Input(kUnrelated));
    EXPECT_EQ(expected_qwacness, Has2QwacPolicies(policy_set));
  }
}

TEST(Has2QwacEku, TestEkuCases) {
  struct TestCase {
    QwacEkuStatus expected_qwacness;
    std::vector<bssl::der::Input> ekus;
  } kTestCases[] = {
      // Expected QWAC case:
      {QwacEkuStatus::kHasQwacEku, {bssl::der::Input(kIdKpTlsBinding)}},
      // Contains additional EKUs:
      {QwacEkuStatus::kInconsistent,
       {bssl::der::Input(kIdKpTlsBinding),
        bssl::der::Input(bssl::kServerAuth)}},
      // Wrong eku:
      {QwacEkuStatus::kNotQwac, {bssl::der::Input(bssl::kServerAuth)}},
      {QwacEkuStatus::kNotQwac,
       {bssl::der::Input(bssl::kServerAuth),
        bssl::der::Input(bssl::kClientAuth)}},
      // No eku:
      {QwacEkuStatus::kNotQwac, {}},
  };

  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  for (const auto& [expected_qwacness, ekus] : kTestCases) {
    if (ekus.empty()) {
      leaf->EraseExtension(bssl::der::Input(bssl::kExtKeyUsageOid));
    } else {
      leaf->SetExtendedKeyUsages(ekus);
    }
    auto parsed_cert =
        bssl::ParsedCertificate::Create(leaf->DupCertBuffer(), {}, nullptr);
    ASSERT_TRUE(parsed_cert);
    EXPECT_EQ(expected_qwacness, Has2QwacEku(parsed_cert.get()));
  }
}

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
