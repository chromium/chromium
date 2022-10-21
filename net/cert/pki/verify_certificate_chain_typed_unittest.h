// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_PKI_VERIFY_CERTIFICATE_CHAIN_TYPED_UNITTEST_H_
#define NET_CERT_PKI_VERIFY_CERTIFICATE_CHAIN_TYPED_UNITTEST_H_

#include "net/cert/pem.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/pki/test_helpers.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/pki/verify_certificate_chain.h"
#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

template <typename TestDelegate>
class VerifyCertificateChainTest : public ::testing::Test {
 public:
  void RunTest(const char* file_name) {
    VerifyCertChainTest test;

    std::string path =
        std::string("net/data/verify_certificate_chain_unittest/") + file_name;

    SCOPED_TRACE("Test file: " + path);

    if (!ReadVerifyCertChainTestFromFile(path, &test)) {
      ADD_FAILURE() << "Couldn't load test case: " << path;
      return;
    }

    TestDelegate::Verify(test, path);
  }
};

// Tests that have only one root. These can be tested without requiring any
// path-building ability.
template <typename TestDelegate>
class VerifyCertificateChainSingleRootTest
    : public VerifyCertificateChainTest<TestDelegate> {};

TYPED_TEST_SUITE_P(VerifyCertificateChainSingleRootTest);

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, Simple) {
  this->RunTest("target-and-intermediate/main.test");
  this->RunTest("target-and-intermediate/ta-with-expiration.test");
  this->RunTest("target-and-intermediate/ta-with-constraints.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, BasicConstraintsCa) {
  this->RunTest("intermediate-lacks-basic-constraints/main.test");
  this->RunTest("intermediate-basic-constraints-ca-false/main.test");
  this->RunTest("intermediate-basic-constraints-not-critical/main.test");
  this->RunTest("root-lacks-basic-constraints/main.test");
  this->RunTest("root-lacks-basic-constraints/ta-with-constraints.test");
  this->RunTest("root-basic-constraints-ca-false/main.test");
  this->RunTest("root-basic-constraints-ca-false/ta-with-constraints.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, BasicConstraintsPathlen) {
  this->RunTest("violates-basic-constraints-pathlen-0/main.test");
  this->RunTest("basic-constraints-pathlen-0-self-issued/main.test");
  this->RunTest("target-has-pathlen-but-not-ca/main.test");
  this->RunTest("violates-pathlen-1-from-root/main.test");
  this->RunTest("violates-pathlen-1-from-root/ta-with-constraints.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, UnknownExtension) {
  this->RunTest("intermediate-unknown-critical-extension/main.test");
  this->RunTest("intermediate-unknown-non-critical-extension/main.test");
  this->RunTest("target-unknown-critical-extension/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, WeakSignature) {
  this->RunTest("target-signed-with-sha1/main.test");
  this->RunTest("intermediate-signed-with-sha1/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, WrongSignature) {
  this->RunTest("target-wrong-signature/main.test");
  this->RunTest("intermediate-and-target-wrong-signature/main.test");
  this->RunTest("incorrect-trust-anchor/main.test");
  this->RunTest("target-wrong-signature-no-authority-key-identifier/main.test");
  this->RunTest(
      "intermediate-wrong-signature-no-authority-key-identifier/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, LastCertificateNotTrusted) {
  this->RunTest("target-and-intermediate/distrusted-root.test");
  this->RunTest("target-and-intermediate/distrusted-root-expired.test");
  this->RunTest("target-and-intermediate/unspecified-trust-root.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, WeakPublicKey) {
  this->RunTest("target-signed-by-512bit-rsa/main.test");
  this->RunTest("target-has-512bit-rsa-key/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, TargetSignedUsingEcdsa) {
  this->RunTest("target-signed-using-ecdsa/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, Expired) {
  this->RunTest("expired-target/not-before.test");
  this->RunTest("expired-target/not-after.test");
  this->RunTest("expired-intermediate/not-before.test");
  this->RunTest("expired-intermediate/not-after.test");
  this->RunTest("expired-root/not-before.test");
  this->RunTest("expired-root/not-before-ta-with-expiration.test");
  this->RunTest("expired-root/not-after.test");
  this->RunTest("expired-root/not-after-ta-with-expiration.test");
  this->RunTest("expired-root/not-after-ta-with-constraints.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, TargetNotEndEntity) {
  this->RunTest("target-not-end-entity/main.test");
  this->RunTest("target-not-end-entity/strict.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, KeyUsage) {
  this->RunTest("intermediate-lacks-signing-key-usage/main.test");
  this->RunTest("target-has-keycertsign-but-not-ca/main.test");

  this->RunTest("target-serverauth-various-keyusages/rsa-decipherOnly.test");
  this->RunTest(
      "target-serverauth-various-keyusages/rsa-digitalSignature.test");
  this->RunTest("target-serverauth-various-keyusages/rsa-keyAgreement.test");
  this->RunTest("target-serverauth-various-keyusages/rsa-keyEncipherment.test");

  this->RunTest("target-serverauth-various-keyusages/ec-decipherOnly.test");
  this->RunTest("target-serverauth-various-keyusages/ec-digitalSignature.test");
  this->RunTest("target-serverauth-various-keyusages/ec-keyAgreement.test");
  this->RunTest("target-serverauth-various-keyusages/ec-keyEncipherment.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, ExtendedKeyUsage) {
  this->RunTest("intermediate-eku-clientauth/any.test");
  this->RunTest("intermediate-eku-clientauth/serverauth.test");
  this->RunTest("intermediate-eku-clientauth/clientauth.test");
  this->RunTest("intermediate-eku-clientauth/serverauth-strict.test");
  this->RunTest("intermediate-eku-clientauth/clientauth-strict.test");
  this->RunTest("intermediate-eku-any-and-clientauth/any.test");
  this->RunTest("intermediate-eku-any-and-clientauth/serverauth.test");
  this->RunTest("intermediate-eku-any-and-clientauth/serverauth-strict.test");
  this->RunTest("intermediate-eku-any-and-clientauth/clientauth.test");
  this->RunTest("intermediate-eku-any-and-clientauth/clientauth-strict.test");
  this->RunTest("target-eku-clientauth/any.test");
  this->RunTest("target-eku-clientauth/serverauth.test");
  this->RunTest("target-eku-clientauth/clientauth.test");
  this->RunTest("target-eku-clientauth/serverauth-strict.test");
  this->RunTest("target-eku-clientauth/clientauth-strict.test");
  this->RunTest("target-eku-any/any.test");
  this->RunTest("target-eku-any/serverauth.test");
  this->RunTest("target-eku-any/clientauth.test");
  this->RunTest("target-eku-any/serverauth-strict.test");
  this->RunTest("target-eku-any/clientauth-strict.test");
  this->RunTest("target-eku-many/any.test");
  this->RunTest("target-eku-many/serverauth.test");
  this->RunTest("target-eku-many/clientauth.test");
  this->RunTest("target-eku-many/serverauth-strict.test");
  this->RunTest("target-eku-many/clientauth-strict.test");
  this->RunTest("target-eku-none/any.test");
  this->RunTest("target-eku-none/serverauth.test");
  this->RunTest("target-eku-none/clientauth.test");
  this->RunTest("target-eku-none/serverauth-strict.test");
  this->RunTest("target-eku-none/clientauth-strict.test");
  this->RunTest("root-eku-clientauth/serverauth.test");
  this->RunTest("root-eku-clientauth/serverauth-strict.test");
  this->RunTest("root-eku-clientauth/serverauth-ta-with-constraints.test");
  this->RunTest(
      "root-eku-clientauth/serverauth-ta-with-constraints-strict.test");
  this->RunTest("intermediate-eku-server-gated-crypto/sha1-eku-any.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha1-eku-clientAuth.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha1-eku-clientAuth-strict.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha1-eku-serverAuth.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha1-eku-serverAuth-strict.test");
  this->RunTest("intermediate-eku-server-gated-crypto/sha256-eku-any.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha256-eku-clientAuth-strict.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha256-eku-serverAuth.test");
  this->RunTest(
      "intermediate-eku-server-gated-crypto/sha256-eku-serverAuth-strict.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest,
             IssuerAndSubjectNotByteForByteEqual) {
  this->RunTest("issuer-and-subject-not-byte-for-byte-equal/target.test");
  this->RunTest("issuer-and-subject-not-byte-for-byte-equal/anchor.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, TrustAnchorNotSelfSigned) {
  this->RunTest("non-self-signed-root/main.test");
  this->RunTest("non-self-signed-root/ta-with-constraints.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, KeyRollover) {
  this->RunTest("key-rollover/oldchain.test");
  this->RunTest("key-rollover/rolloverchain.test");
  this->RunTest("key-rollover/longrolloverchain.test");
  this->RunTest("key-rollover/newchain.test");
}

// Test coverage of policies comes primarily from the PKITS tests. The
// tests here only cover aspects not already tested by PKITS.
TYPED_TEST_P(VerifyCertificateChainSingleRootTest, Policies) {
  this->RunTest("unknown-critical-policy-qualifier/main.test");
  this->RunTest("unknown-non-critical-policy-qualifier/main.test");
}

TYPED_TEST_P(VerifyCertificateChainSingleRootTest, ManyNames) {
  this->RunTest("many-names/ok-all-types.test");
  this->RunTest("many-names/ok-different-types-dns.test");
  this->RunTest("many-names/ok-different-types-ips.test");
  this->RunTest("many-names/ok-different-types-dirnames.test");
  this->RunTest("many-names/toomany-all-types.test");
  this->RunTest("many-names/toomany-dns-excluded.test");
  this->RunTest("many-names/toomany-dns-permitted.test");
  this->RunTest("many-names/toomany-ips-excluded.test");
  this->RunTest("many-names/toomany-ips-permitted.test");
  this->RunTest("many-names/toomany-dirnames-excluded.test");
  this->RunTest("many-names/toomany-dirnames-permitted.test");
}

// TODO(eroman): Add test that invalid validity dates where the day or month
// ordinal not in range, like "March 39, 2016" are rejected.

REGISTER_TYPED_TEST_SUITE_P(VerifyCertificateChainSingleRootTest,
                            Simple,
                            BasicConstraintsCa,
                            BasicConstraintsPathlen,
                            UnknownExtension,
                            WeakSignature,
                            WrongSignature,
                            LastCertificateNotTrusted,
                            WeakPublicKey,
                            TargetSignedUsingEcdsa,
                            Expired,
                            TargetNotEndEntity,
                            KeyUsage,
                            ExtendedKeyUsage,
                            IssuerAndSubjectNotByteForByteEqual,
                            TrustAnchorNotSelfSigned,
                            KeyRollover,
                            Policies,
                            ManyNames);

}  // namespace net

#endif  // NET_CERT_PKI_VERIFY_CERTIFICATE_CHAIN_TYPED_UNITTEST_H_
