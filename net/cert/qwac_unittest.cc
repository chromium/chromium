// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/qwac.h"

#include <stdint.h>

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

}  // namespace
}  // namespace net
