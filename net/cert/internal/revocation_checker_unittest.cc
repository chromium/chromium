// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/revocation_checker.h"

#include <string_view>

#include "base/time/time.h"
#include "net/cert/mock_cert_net_fetcher.h"
#include "net/test/cert_builder.h"
#include "net/test/revocation_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/common_cert_errors.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

bool AddCertsToList(std::vector<CertBuilder*> builders,
                    bssl::ParsedCertificateList* out_certs) {
  for (auto* builder : builders) {
    if (!bssl::ParsedCertificate::CreateAndAddToVector(
            builder->DupCertBuffer(), {}, out_certs, /*errors=*/nullptr)) {
      return false;
    }
  }
  return true;
}

TEST(RevocationChecker, NoRevocationMechanism) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;

  {
    // Require revocation methods to be presented.
    policy.allow_missing_info = false;

    // No methods on |mock_fetcher| should be called.
    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(
        errors.ContainsError(bssl::cert_errors::kNoRevocationMechanism));
  }

  {
    // Allow certs without revocation methods.
    policy.allow_missing_info = true;

    // No methods on |mock_fetcher| should be called.
    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors,
        /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_FALSE(errors.ContainsHighSeverityErrors());
  }

  {
    // Revocation checking disabled.
    policy.check_revocation = false;
    // Require revocation methods to be presented, but this does not matter if
    // check_revocation is false.
    policy.allow_missing_info = false;

    // No methods on |mock_fetcher| should be called.
    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_FALSE(errors.ContainsHighSeverityErrors());
  }
}

TEST(RevocationChecker, ValidCRL) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kTestCrlUrl("http://example.com/crl1");
  leaf->SetCrlDistributionPointUrl(kTestCrlUrl);

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.allow_missing_info = false;
  policy.allow_unable_to_check = false;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  {
    policy.networking_allowed = true;
    policy.crl_allowed = true;

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_FALSE(errors.ContainsHighSeverityErrors());
  }

  {
    policy.networking_allowed = false;
    policy.crl_allowed = true;

    // No methods on |mock_fetcher| should be called.
    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(
        errors.ContainsError(bssl::cert_errors::kUnableToCheckRevocation));
  }

  {
    policy.networking_allowed = true;
    policy.crl_allowed = false;

    // No methods on |mock_fetcher| should be called.
    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    // Since CRLs were not considered, the error should be "no revocation
    // mechanism".
    EXPECT_TRUE(
        errors.ContainsError(bssl::cert_errors::kNoRevocationMechanism));
  }
}

TEST(RevocationChecker, RevokedCRL) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kTestCrlUrl("http://example.com/crl1");
  leaf->SetCrlDistributionPointUrl(kTestCrlUrl);

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{leaf->GetSerialNumber()});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  {
    // These should have no effect on an affirmatively revoked response.
    policy.allow_missing_info = false;
    policy.allow_unable_to_check = false;

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(errors.ContainsError(bssl::cert_errors::kCertificateRevoked));
  }

  {
    // These should have no effect on an affirmatively revoked response.
    policy.allow_missing_info = true;
    policy.allow_unable_to_check = true;

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(errors.ContainsError(bssl::cert_errors::kCertificateRevoked));
  }
}

TEST(RevocationChecker, CRLRequestFails) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kTestCrlUrl("http://example.com/crl1");
  leaf->SetCrlDistributionPointUrl(kTestCrlUrl);

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;

  {
    policy.allow_unable_to_check = false;
    policy.allow_missing_info = false;

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(
            ByMove(MockCertNetFetcherRequest::Create(ERR_CONNECTION_FAILED))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(
        errors.ContainsError(bssl::cert_errors::kUnableToCheckRevocation));
  }

  {
    policy.allow_unable_to_check = false;
    policy.allow_missing_info = true;  // Should have no effect.

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(
            ByMove(MockCertNetFetcherRequest::Create(ERR_CONNECTION_FAILED))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_TRUE(errors.ContainsHighSeverityErrors());
    EXPECT_TRUE(
        errors.ContainsError(bssl::cert_errors::kUnableToCheckRevocation));
  }

  {
    policy.allow_unable_to_check = true;
    policy.allow_missing_info = false;

    auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
    EXPECT_CALL(*mock_fetcher, FetchCrl(kTestCrlUrl, _, _))
        .WillOnce(Return(
            ByMove(MockCertNetFetcherRequest::Create(ERR_CONNECTION_FAILED))));

    bssl::CertPathErrors errors;
    CheckValidatedChainRevocation(
        chain, policy, /*deadline=*/base::TimeTicks(),
        /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
        mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

    EXPECT_FALSE(errors.ContainsHighSeverityErrors());
  }
}

TEST(RevocationChecker, CRLNonHttpUrl) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kTestCrlUrl("https://example.com/crl1");
  leaf->SetCrlDistributionPointUrl(kTestCrlUrl);

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;
  policy.allow_missing_info = false;

  // HTTPS CRL URLs should not be fetched.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  bssl::CertPathErrors errors;
  CheckValidatedChainRevocation(
      chain, policy, /*deadline=*/base::TimeTicks(),
      /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
      mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

  EXPECT_TRUE(errors.ContainsHighSeverityErrors());
  EXPECT_TRUE(errors.ContainsError(bssl::cert_errors::kNoRevocationMechanism));
}

TEST(RevocationChecker, SkipEntireInvalidCRLDistributionPoints) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kSecondCrlUrl("http://www.example.com/bar.crl");

  // SEQUENCE {
  //   # First distribution point: this is invalid, thus the entire
  //   # crlDistributionPoints extension should be ignored and revocation
  //   # checking should fail.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         # [9] is not a valid tag in bssl::GeneralNames
  //         [9 PRIMITIVE] { "foo" }
  //       }
  //     }
  //   }
  //   # Second distribution point. Even though this is an acceptable
  //   # distributionPoint, it should not be used.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/bar.crl" }
  //       }
  //     }
  //   }
  // }
  const uint8_t crldp[] = {0x30, 0x31, 0x30, 0x09, 0xa0, 0x07, 0xa0, 0x05, 0x89,
                           0x03, 0x66, 0x6f, 0x6f, 0x30, 0x24, 0xa0, 0x22, 0xa0,
                           0x20, 0x86, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f,
                           0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d,
                           0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x62,
                           0x61, 0x72, 0x2e, 0x63, 0x72, 0x6c};
  leaf->SetExtension(
      bssl::der::Input(bssl::kCrlDistributionPointsOid),
      std::string(reinterpret_cast<const char*>(crldp), std::size(crldp)));

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;
  policy.allow_missing_info = false;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  // No methods on |mock_fetcher| should be called.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();

  bssl::CertPathErrors errors;
  CheckValidatedChainRevocation(
      chain, policy, /*deadline=*/base::TimeTicks(),
      /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
      mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

  // Should fail since the entire cRLDistributionPoints extension was skipped
  // and no other revocation method is present.
  EXPECT_TRUE(errors.ContainsHighSeverityErrors());
  EXPECT_TRUE(errors.ContainsError(bssl::cert_errors::kNoRevocationMechanism));
}

TEST(RevocationChecker, SkipUnsupportedCRLDistPointWithNonUriFullname) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kSecondCrlUrl("http://www.example.com/bar.crl");

  // SEQUENCE {
  //   # First distribution point: this should be ignored since it has a non-URI
  //   # fullName field.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [4] {
  //           SEQUENCE {
  //             SET {
  //               SEQUENCE {
  //                 # countryName
  //                 OBJECT_IDENTIFIER { 2.5.4.6 }
  //                 PrintableString { "US" }
  //               }
  //             }
  //             SET {
  //               SEQUENCE {
  //                 # commonName
  //                 OBJECT_IDENTIFIER { 2.5.4.3 }
  //                 PrintableString { "foo" }
  //               }
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }
  //   # Second distribution point. This should be used since it only has a
  //   # fullName URI.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/bar.crl" }
  //       }
  //     }
  //   }
  // }
  const uint8_t crldp[] = {
      0x30, 0x4b, 0x30, 0x23, 0xa0, 0x21, 0xa0, 0x1f, 0xa4, 0x1d, 0x30,
      0x1b, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x55, 0x53, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04,
      0x03, 0x13, 0x03, 0x66, 0x6f, 0x6f, 0x30, 0x24, 0xa0, 0x22, 0xa0,
      0x20, 0x86, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77,
      0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
      0x63, 0x6f, 0x6d, 0x2f, 0x62, 0x61, 0x72, 0x2e, 0x63, 0x72, 0x6c};
  leaf->SetExtension(
      bssl::der::Input(bssl::kCrlDistributionPointsOid),
      std::string(reinterpret_cast<const char*>(crldp), std::size(crldp)));

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;
  policy.allow_missing_info = false;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  // The first crldp should be skipped, the second should be retrieved.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  EXPECT_CALL(*mock_fetcher, FetchCrl(kSecondCrlUrl, _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

  bssl::CertPathErrors errors;
  CheckValidatedChainRevocation(
      chain, policy, /*deadline=*/base::TimeTicks(),
      /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
      mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

  EXPECT_FALSE(errors.ContainsHighSeverityErrors());
}

TEST(RevocationChecker, SkipUnsupportedCRLDistPointWithReasons) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kSecondCrlUrl("http://www.example.com/bar.crl");

  // SEQUENCE {
  //   # First distribution point: this should be ignored since it has a reasons
  //   # field.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/foo.crl" }
  //       }
  //     }
  //     # reasons
  //     [1 PRIMITIVE] { b`011` }
  //   }
  //   # Second distribution point. This should be used since it only has a
  //   # fullName URI.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/bar.crl" }
  //       }
  //     }
  //   }
  // }
  const uint8_t crldp[] = {
      0x30, 0x50, 0x30, 0x28, 0xa0, 0x22, 0xa0, 0x20, 0x86, 0x1e, 0x68, 0x74,
      0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65, 0x78, 0x61,
      0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x66, 0x6f, 0x6f,
      0x2e, 0x63, 0x72, 0x6c, 0x81, 0x02, 0x05, 0x60, 0x30, 0x24, 0xa0, 0x22,
      0xa0, 0x20, 0x86, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77,
      0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63,
      0x6f, 0x6d, 0x2f, 0x62, 0x61, 0x72, 0x2e, 0x63, 0x72, 0x6c};
  leaf->SetExtension(
      bssl::der::Input(bssl::kCrlDistributionPointsOid),
      std::string(reinterpret_cast<const char*>(crldp), std::size(crldp)));

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;
  policy.allow_missing_info = false;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  // The first crldp should be skipped, the second should be retrieved.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  EXPECT_CALL(*mock_fetcher, FetchCrl(kSecondCrlUrl, _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

  bssl::CertPathErrors errors;
  CheckValidatedChainRevocation(
      chain, policy, /*deadline=*/base::TimeTicks(),
      /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
      mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

  EXPECT_FALSE(errors.ContainsHighSeverityErrors());
}

TEST(RevocationChecker, SkipUnsupportedCRLDistPointWithCrlIssuer) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  const GURL kSecondCrlUrl("http://www.example.com/bar.crl");

  // SEQUENCE {
  //   # First distribution point: this should be ignored since it has a
  //   crlIssuer field.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/foo.crl" }
  //       }
  //     }
  //     [2] {
  //       [4] {
  //         SEQUENCE {
  //           SET {
  //             SEQUENCE {
  //               # countryName
  //               OBJECT_IDENTIFIER { 2.5.4.6 }
  //               PrintableString { "US" }
  //             }
  //           }
  //           SET {
  //             SEQUENCE {
  //               # organizationName
  //               OBJECT_IDENTIFIER { 2.5.4.10 }
  //               PrintableString { "Test Certificates 2011" }
  //             }
  //           }
  //           SET {
  //             SEQUENCE {
  //               # organizationUnitName
  //               OBJECT_IDENTIFIER { 2.5.4.11 }
  //               PrintableString { "indirectCRL CA3 cRLIssuer" }
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }
  //   # Second distribution point. This should be used since it only has a
  //   # fullName URI.
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [6 PRIMITIVE] { "http://www.example.com/bar.crl" }
  //       }
  //     }
  //   }
  // }
  const uint8_t crldp[] = {
      0x30, 0x81, 0xa4, 0x30, 0x7c, 0xa0, 0x22, 0xa0, 0x20, 0x86, 0x1e, 0x68,
      0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x65, 0x78,
      0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x66, 0x6f,
      0x6f, 0x2e, 0x63, 0x72, 0x6c, 0xa2, 0x56, 0xa4, 0x54, 0x30, 0x52, 0x31,
      0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53,
      0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x16, 0x54,
      0x65, 0x73, 0x74, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63,
      0x61, 0x74, 0x65, 0x73, 0x20, 0x32, 0x30, 0x31, 0x31, 0x31, 0x22, 0x30,
      0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x19, 0x69, 0x6e, 0x64, 0x69,
      0x72, 0x65, 0x63, 0x74, 0x43, 0x52, 0x4c, 0x20, 0x43, 0x41, 0x33, 0x20,
      0x63, 0x52, 0x4c, 0x49, 0x73, 0x73, 0x75, 0x65, 0x72, 0x30, 0x24, 0xa0,
      0x22, 0xa0, 0x20, 0x86, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
      0x77, 0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
      0x63, 0x6f, 0x6d, 0x2f, 0x62, 0x61, 0x72, 0x2e, 0x63, 0x72, 0x6c};
  leaf->SetExtension(
      bssl::der::Input(bssl::kCrlDistributionPointsOid),
      std::string(reinterpret_cast<const char*>(crldp), std::size(crldp)));

  bssl::ParsedCertificateList chain;
  ASSERT_TRUE(AddCertsToList({leaf.get(), root.get()}, &chain));

  RevocationPolicy policy;
  policy.check_revocation = true;
  policy.networking_allowed = true;
  policy.crl_allowed = true;
  policy.allow_unable_to_check = false;
  policy.allow_missing_info = false;

  std::string crl_data_as_string_for_some_reason =
      BuildCrl(root->GetSubject(), root->GetKey(),
               /*revoked_serials=*/{});
  std::vector<uint8_t> crl_data(crl_data_as_string_for_some_reason.begin(),
                                crl_data_as_string_for_some_reason.end());

  // The first crldp should be skipped, the second should be retrieved.
  auto mock_fetcher = base::MakeRefCounted<StrictMock<MockCertNetFetcher>>();
  EXPECT_CALL(*mock_fetcher, FetchCrl(kSecondCrlUrl, _, _))
      .WillOnce(Return(ByMove(MockCertNetFetcherRequest::Create(crl_data))));

  bssl::CertPathErrors errors;
  CheckValidatedChainRevocation(
      chain, policy, /*deadline=*/base::TimeTicks(),
      /*stapled_leaf_ocsp_response=*/std::string_view(), base::Time::Now(),
      mock_fetcher.get(), &errors, /*stapled_ocsp_verify_result=*/nullptr);

  EXPECT_FALSE(errors.ContainsHighSeverityErrors());
}

// TODO(mattm): Add more unittests (deadlines, OCSP, stapled OCSP, CRLSets).
// Currently those features are exercised indirectly through tests in
// url_request_unittest.cc, cert_verify_proc_unittest.cc, etc.

}  // namespace

}  // namespace net
