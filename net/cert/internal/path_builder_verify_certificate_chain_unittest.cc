// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/path_builder.h"

#include "net/cert/internal/cert_issuer_source_static.h"
#include "net/cert/internal/simple_path_builder_delegate.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "net/cert/internal/verify_certificate_chain_typed_unittest.h"

namespace net {

namespace {

class PathBuilderTestDelegate {
 public:
  static void Verify(const VerifyCertChainTest& test,
                     const std::string& test_file_path) {
    SimplePathBuilderDelegate path_builder_delegate(
        1024, SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
    ASSERT_FALSE(test.chain.empty());

    TrustStoreInMemory trust_store;

    switch (test.last_cert_trust.type) {
      case CertificateTrustType::TRUSTED_ANCHOR:
        trust_store.AddTrustAnchor(test.chain.back());
        break;
      case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
        trust_store.AddTrustAnchorWithConstraints(test.chain.back());
        break;
      case CertificateTrustType::UNSPECIFIED:
        trust_store.AddCertificateWithUnspecifiedTrust(test.chain.back());
        break;
      case CertificateTrustType::DISTRUSTED:
        trust_store.AddDistrustedCertificateForTest(test.chain.back());
        break;
    }

    CertIssuerSourceStatic intermediate_cert_issuer_source;
    for (size_t i = 1; i < test.chain.size(); ++i)
      intermediate_cert_issuer_source.AddCert(test.chain[i]);

    // First cert in the |chain| is the target.
    CertPathBuilder path_builder(
        test.chain.front(), &trust_store, &path_builder_delegate, test.time,
        test.key_purpose, test.initial_explicit_policy,
        test.user_initial_policy_set, test.initial_policy_mapping_inhibit,
        test.initial_any_policy_inhibit);
    path_builder.AddCertIssuerSource(&intermediate_cert_issuer_source);

    CertPathBuilder::Result result = path_builder.Run();
    EXPECT_EQ(!test.HasHighSeverityErrors(), result.HasValidPath());
  }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(PathBuilder,
                               VerifyCertificateChainSingleRootTest,
                               PathBuilderTestDelegate);

}  // namespace net
