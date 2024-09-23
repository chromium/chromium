// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/do_nothing_ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/revocation_builder.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/ocsp_revocation_status.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "third_party/boringssl/src/pki/trust_store.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#elif BUILDFLAG(IS_IOS)
#include "base/ios/ios_util.h"
#include "net/cert/cert_verify_proc_ios.h"
#elif BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif

using net::test::IsError;
using net::test::IsOk;

using base::HexEncode;

namespace net {

namespace {

const char kTrustAnchorVerifyHistogram[] = "Net.Certificate.TrustAnchor.Verify";
const char kTrustAnchorVerifyOutOfDateHistogram[] =
    "Net.Certificate.TrustAnchor.VerifyOutOfDate";

// Returns a TLV to use as an unknown signature algorithm when building a cert.
// The specific contents are as follows (the OID is from
// https://davidben.net/oid):
//
// SEQUENCE {
//   OBJECT_IDENTIFIER { 1.2.840.113554.4.1.72585.0 }
//   NULL {}
// }
std::string TestOid0SignatureAlgorithmTLV() {
  constexpr uint8_t kTestOid0SigAlgTLV[] = {0x30, 0x10, 0x06, 0x0c, 0x2a, 0x86,
                                            0x48, 0x86, 0xf7, 0x12, 0x04, 0x01,
                                            0x84, 0xb7, 0x09, 0x00, 0x05, 0x00};
  return std::string(std::begin(kTestOid0SigAlgTLV),
                     std::end(kTestOid0SigAlgTLV));
}

// An OID for use in tests, from https://davidben.net/oid
// OBJECT_IDENTIFIER { 1.2.840.113554.4.1.72585.0 }
bssl::der::Input TestOid0() {
  static uint8_t kTestOid0[] = {0x06, 0x0c, 0x2a, 0x86, 0x48, 0x86, 0xf7,
                                0x12, 0x04, 0x01, 0x84, 0xb7, 0x09, 0x00};
  return bssl::der::Input(kTestOid0);
}

// Mock CertVerifyProc that sets the CertVerifyResult to a given value for
// all certificates that are Verify()'d
class MockCertVerifyProc : public CertVerifyProc {
 public:
  explicit MockCertVerifyProc(const CertVerifyResult& result)
      : CertVerifyProc(CRLSet::BuiltinCRLSet()), result_(result) {}
  MockCertVerifyProc(const CertVerifyResult& result, int error)
      : CertVerifyProc(CRLSet::BuiltinCRLSet()),
        result_(result),
        error_(error) {}

  MockCertVerifyProc(const MockCertVerifyProc&) = delete;
  MockCertVerifyProc& operator=(const MockCertVerifyProc&) = delete;

 protected:
  ~MockCertVerifyProc() override = default;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  const CertVerifyResult result_;
  const int error_ = OK;
};

int MockCertVerifyProc::VerifyInternal(X509Certificate* cert,
                                       const std::string& hostname,
                                       const std::string& ocsp_response,
                                       const std::string& sct_list,
                                       int flags,
                                       CertVerifyResult* verify_result,
                                       const NetLogWithSource& net_log) {
  *verify_result = result_;
  verify_result->verified_cert = cert;
  return error_;
}

// This enum identifies a concrete implemenation of CertVerifyProc.
//
// The type is erased by CreateCertVerifyProc(), however needs to be known for
// some of the test expectations.
enum CertVerifyProcType {
  CERT_VERIFY_PROC_ANDROID,
  CERT_VERIFY_PROC_IOS,
  CERT_VERIFY_PROC_BUILTIN,
  CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS,
};

// Returns a textual description of the CertVerifyProc implementation
// that is being tested, used to give better names to parameterized
// tests.
std::string VerifyProcTypeToName(
    const testing::TestParamInfo<CertVerifyProcType>& params) {
  switch (params.param) {
    case CERT_VERIFY_PROC_ANDROID:
      return "CertVerifyProcAndroid";
    case CERT_VERIFY_PROC_IOS:
      return "CertVerifyProcIOS";
    case CERT_VERIFY_PROC_BUILTIN:
      return "CertVerifyProcBuiltin";
    case CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS:
      return "CertVerifyProcBuiltinChromeRoots";
  }

  return "";
}

scoped_refptr<CertVerifyProc> CreateCertVerifyProc(
    CertVerifyProcType type,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    CertificateList additional_trust_anchors,
    CertificateList additional_untrusted_authorities) {
  CertVerifyProc::InstanceParams instance_params;
  instance_params.additional_trust_anchors =
      net::x509_util::ParseAllValidCerts(additional_trust_anchors);
  instance_params.additional_untrusted_authorities =
      net::x509_util::ParseAllValidCerts(additional_untrusted_authorities);
  switch (type) {
#if BUILDFLAG(IS_ANDROID)
    case CERT_VERIFY_PROC_ANDROID:
      return base::MakeRefCounted<CertVerifyProcAndroid>(
          std::move(cert_net_fetcher), std::move(crl_set));
#elif BUILDFLAG(IS_IOS)
    case CERT_VERIFY_PROC_IOS:
      return base::MakeRefCounted<CertVerifyProcIOS>(std::move(crl_set));
#endif
#if BUILDFLAG(IS_FUCHSIA)
    case CERT_VERIFY_PROC_BUILTIN:
      return CreateCertVerifyProcBuiltin(
          std::move(cert_net_fetcher), std::move(crl_set),
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          CreateSslSystemTrustStore(), instance_params, std::nullopt);
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    case CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS:
      return CreateCertVerifyProcBuiltin(
          std::move(cert_net_fetcher), std::move(crl_set),
          std::make_unique<DoNothingCTVerifier>(),
          base::MakeRefCounted<DefaultCTPolicyEnforcer>(),
          CreateSslSystemTrustStoreChromeRoot(
              std::make_unique<net::TrustStoreChrome>()),
          instance_params, std::nullopt);
#endif
    default:
      return nullptr;
  }
}

// The set of all CertVerifyProcTypes that tests should be parameterized on.
// This needs to be kept in sync with CertVerifyProc::CreateSystemVerifyProc()
// and the platforms where CreateSslSystemTrustStore() is not a dummy store.
constexpr CertVerifyProcType kAllCertVerifiers[] = {
#if BUILDFLAG(IS_ANDROID)
    CERT_VERIFY_PROC_ANDROID,
#elif BUILDFLAG(IS_IOS)
    CERT_VERIFY_PROC_IOS,
#elif BUILDFLAG(IS_FUCHSIA)
    CERT_VERIFY_PROC_BUILTIN,
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS,
#endif
};
static_assert(std::size(kAllCertVerifiers) != 0, "Unsupported platform");

// Returns true if a test root added through ScopedTestRoot can verify
// successfully as a target certificate with chain of length 1 on the given
// CertVerifyProcType.
bool ScopedTestRootCanTrustTargetCert(CertVerifyProcType verify_proc_type) {
  return verify_proc_type == CERT_VERIFY_PROC_IOS ||
         verify_proc_type == CERT_VERIFY_PROC_ANDROID;
}

// Returns true if a non-self-signed CA certificate added through
// ScopedTestRoot can verify successfully as the root of a chain by the given
// CertVerifyProcType.
bool ScopedTestRootCanTrustIntermediateCert(
    CertVerifyProcType verify_proc_type) {
  return verify_proc_type == CERT_VERIFY_PROC_IOS ||
         verify_proc_type == CERT_VERIFY_PROC_BUILTIN ||
         verify_proc_type == CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS ||
         verify_proc_type == CERT_VERIFY_PROC_ANDROID;
}

std::string MakeRandomHexString(size_t num_bytes) {
  std::vector<uint8_t> rand_bytes(num_bytes);
  base::RandBytes(rand_bytes);
  return base::HexEncode(rand_bytes);
}

}  // namespace

// This fixture is for tests that apply to concrete implementations of
// CertVerifyProc. It will be run for all of the concrete CertVerifyProc types.
//
// It is called "Internal" as it tests the internal methods like
// "VerifyInternal()".
class CertVerifyProcInternalTest
    : public testing::TestWithParam<CertVerifyProcType> {
 protected:
  void SetUp() override { SetUpCertVerifyProc(CRLSet::BuiltinCRLSet()); }

  // CertNetFetcher may be initialized by subclasses that want to use net
  // fetching by calling SetUpWithCertNetFetcher instead of SetUp.
  void SetUpWithCertNetFetcher(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set,
      CertificateList additional_trust_anchors,
      CertificateList additional_untrusted_authorities) {
    CertVerifyProcType type = verify_proc_type();
    verify_proc_ = CreateCertVerifyProc(
        type, std::move(cert_net_fetcher), std::move(crl_set),
        additional_trust_anchors, additional_untrusted_authorities);
    ASSERT_TRUE(verify_proc_);
  }

  virtual void SetUpCertVerifyProc(scoped_refptr<CRLSet> crl_set) {
    SetUpWithCertNetFetcher(nullptr, std::move(crl_set),
                            /*additional_trust_anchors=*/{},
                            /*additional_untrusted_authorities=*/{});
  }

  virtual void SetUpWithAdditionalCerts(
      CertificateList additional_trust_anchors,
      CertificateList additional_untrusted_authorities) {
    SetUpWithCertNetFetcher(nullptr, CRLSet::BuiltinCRLSet(),
                            additional_trust_anchors,
                            additional_untrusted_authorities);
  }

  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CertVerifyResult* verify_result,
             const NetLogWithSource& net_log) {
    return verify_proc_->Verify(cert, hostname, /*ocsp_response=*/std::string(),
                                /*sct_list=*/std::string(), flags,
                                verify_result, net_log);
  }

  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CertVerifyResult* verify_result) {
    return Verify(cert, hostname, flags, verify_result, NetLogWithSource());
  }

  int Verify(X509Certificate* cert, const std::string& hostname) {
    CertVerifyResult verify_result;
    int flags = 0;
    return Verify(cert, hostname, flags, &verify_result);
  }

  CertVerifyProcType verify_proc_type() const { return GetParam(); }

  // Returns true if the RSA/DSA keysize will be considered weak on the current
  // platform. IsInvalidRsaDsaKeySize should be checked prior, since some very
  // weak keys may be considered invalid.
  bool IsWeakRsaDsaKeySize(int size) const {
#if BUILDFLAG(IS_IOS)
    // Beginning with iOS 13, the minimum key size for RSA/DSA algorithms is
    // 2048 bits. See https://support.apple.com/en-us/HT210176
    if (verify_proc_type() == CERT_VERIFY_PROC_IOS) {
      return size < 2048;
    }
#endif

    return size < 1024;
  }

  // Returns true if the RSA/DSA keysize will be considered invalid on the
  // current platform.
  bool IsInvalidRsaDsaKeySize(int size) const {
#if BUILDFLAG(IS_IOS)
    // On iOS using SecTrustEvaluateWithError it is not possible to
    // distinguish between weak and invalid key sizes.
    return IsWeakRsaDsaKeySize(size);
#else
    // This platform does not mark certificates with weak keys as invalid.
    return false;
#endif
  }

  static bool ParseKeyType(const std::string& key_type,
                           std::string* type,
                           int* size) {
    size_t pos = key_type.find("-");
    *type = key_type.substr(0, pos);
    std::string size_str = key_type.substr(pos + 1);
    return base::StringToInt(size_str, size);
  }

  // Some platforms may reject certificates with very weak keys as invalid.
  bool IsInvalidKeyType(const std::string& key_type) const {
    std::string type;
    int size = 0;
    if (!ParseKeyType(key_type, &type, &size))
      return false;

    if (type == "rsa" || type == "dsa")
      return IsInvalidRsaDsaKeySize(size);

    return false;
  }

  // Currently, only RSA and DSA keys are checked for weakness, and our example
  // weak size is 768. These could change in the future.
  //
  // Note that this means there may be false negatives: keys for other
  // algorithms and which are weak will pass this test.
  //
  // Also, IsInvalidKeyType should be checked prior, since some weak keys may be
  // considered invalid.
  bool IsWeakKeyType(const std::string& key_type) const {
    std::string type;
    int size = 0;
    if (!ParseKeyType(key_type, &type, &size))
      return false;

    if (type == "rsa" || type == "dsa")
      return IsWeakRsaDsaKeySize(size);

    return false;
  }

  bool SupportsCRLSet() const { return VerifyProcTypeIsBuiltin(); }

  bool SupportsCRLSetsInPathBuilding() const {
    return VerifyProcTypeIsBuiltin();
  }

  bool SupportsEV() const {
    // Android and iOS do not support EV.  See https://crbug.com/117478#7
#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
    return true;
#else
    return false;
#endif
  }

  bool SupportsSoftFailRevChecking() const { return VerifyProcTypeIsBuiltin(); }

  bool SupportsRevCheckingRequiredLocalAnchors() const {
    return VerifyProcTypeIsBuiltin();
  }

  bool VerifyProcTypeIsBuiltin() const {
    return verify_proc_type() == CERT_VERIFY_PROC_BUILTIN ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS;
  }

  bool VerifyProcTypeIsIOSAtMostOS14() const {
    return false;
  }

  bool VerifyProcTypeIsIOSAtMostOS15() const {
#if BUILDFLAG(IS_IOS)
    if (verify_proc_type() == CERT_VERIFY_PROC_IOS &&
        !base::ios::IsRunningOnIOS16OrLater()) {
      return true;
    }
#endif
    return false;
  }

  CertVerifyProc* verify_proc() const { return verify_proc_.get(); }

 private:
  scoped_refptr<CertVerifyProc> verify_proc_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcInternalTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

TEST_P(CertVerifyProcInternalTest, DistrustedIntermediate) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  constexpr char kHostname[] = "www.example.com";

  // Chain should not verify without any ScopedTestRoots.
  EXPECT_THAT(Verify(chain.get(), kHostname),
              IsError(ERR_CERT_AUTHORITY_INVALID));

  // Trusting root should cause chain to verify successfully.
  ScopedTestRoot trust_root(root->GetX509Certificate(),
                            bssl::CertificateTrust::ForTrustAnchor());
  EXPECT_THAT(Verify(chain.get(), kHostname), IsOk());

  ScopedTestRoot distrust_intermediate(intermediate->GetX509Certificate(),
                                       bssl::CertificateTrust::ForDistrusted());
  if (VerifyProcTypeIsBuiltin()) {
    // Distrusting intermediate should cause chain to not verify again.
    EXPECT_THAT(Verify(chain.get(), kHostname),
                IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    // Specifying trust types for the platform verifiers through ScopedTestRoot
    // is not supported, so this should still verify successfully.
    EXPECT_THAT(Verify(chain.get(), kHostname), IsOk());
  }
}

// Tests that a certificate is recognized as EV, when the valid EV policy OID
// for the trust anchor is the second candidate EV oid in the target
// certificate. This is a regression test for crbug.com/705285.
TEST_P(CertVerifyProcInternalTest, EVVerificationMultipleOID) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }

  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // The policies that target certificate asserts.
  static const char kOtherTestCertPolicy[] = "2.23.140.1.1";
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  // Specify the extraneous policy first, then the actual policy.
  leaf->SetCertificatePolicies({kOtherTestCertPolicy, kEVTestCertPolicy});

  scoped_refptr<X509Certificate> cert = leaf->GetX509Certificate();
  ScopedTestRoot test_root(root->GetX509Certificate());

  // Build a CRLSet that covers the target certificate.
  //
  // This way CRLSet coverage will be sufficient for EV revocation checking,
  // so this test does not depend on online revocation checking.
  std::string_view spki;
  ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(
      x509_util::CryptoBufferAsStringPiece(root->GetCertBuffer()), &spki));
  SHA256HashValue spki_sha256;
  crypto::SHA256HashString(spki, spki_sha256.data, sizeof(spki_sha256.data));
  SetUpCertVerifyProc(CRLSet::ForTesting(false, &spki_sha256, "", "", {}));

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
      kEVTestCertPolicy);
  ScopedTestEVPolicy scoped_test_other_policy(
      EVRootCAMetadata::GetInstance(), SHA256HashValue(), kOtherTestCertPolicy);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(cert.get(), "www.example.com", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and verifies successfully, but has a chain of
// length 1 because the target cert was directly trusted in the trust store.
// Should verify OK but not with STATUS_IS_EV.
TEST_P(CertVerifyProcInternalTest, TrustedTargetCertWithEVPolicy) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(), SHA256HashValue(), kEVTestCertPolicy);

  scoped_refptr<X509Certificate> cert = leaf->GetX509Certificate();
  ScopedTestRoot scoped_test_root(cert);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(cert.get(), "www.example.com", flags, &verify_result);
  if (ScopedTestRootCanTrustTargetCert(verify_proc_type())) {
    EXPECT_THAT(error, IsOk());
    ASSERT_TRUE(verify_result.verified_cert);
    EXPECT_TRUE(verify_result.verified_cert->intermediate_buffers().empty());
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and verifies successfully with a chain of
// length 1, and its fingerprint matches the cert fingerprint for that ev
// policy. This should never happen in reality, but just test that things don't
// explode if it does.
TEST_P(CertVerifyProcInternalTest,
       TrustedTargetCertWithEVPolicyAndEVFingerprint) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  static const char kEVTestCertPolicy[] = "1.2.3.4";
  leaf->SetCertificatePolicies({kEVTestCertPolicy});
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(leaf->GetCertBuffer()),
      kEVTestCertPolicy);
  scoped_refptr<X509Certificate> cert = leaf->GetX509Certificate();
  ScopedTestRoot scoped_test_root(cert);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(cert.get(), "www.example.com", flags, &verify_result);
  if (ScopedTestRootCanTrustTargetCert(verify_proc_type())) {
    EXPECT_THAT(error, IsOk());
    ASSERT_TRUE(verify_result.verified_cert);
    EXPECT_TRUE(verify_result.verified_cert->intermediate_buffers().empty());
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
  // An EV Root certificate should never be used as an end-entity certificate.
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and has a valid path to the EV root, but the
// intermediate has been trusted directly. Should stop building the path at the
// intermediate and verify OK but not with STATUS_IS_EV.
// See https://crbug.com/979801
TEST_P(CertVerifyProcInternalTest, TrustedIntermediateCertWithEVPolicy) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }
  if (!ScopedTestRootCanTrustIntermediateCert(verify_proc_type())) {
    LOG(INFO) << "Skipping test as intermediate cert cannot be trusted";
    return;
  }

  for (bool trust_the_intermediate : {false, true}) {
    SCOPED_TRACE(trust_the_intermediate);

    // Need to build unique certs for each try otherwise caching can break
    // things.
    auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

    static const char kEVTestCertPolicy[] = "1.2.3.4";
    leaf->SetCertificatePolicies({kEVTestCertPolicy});
    intermediate->SetCertificatePolicies({kEVTestCertPolicy});
    // Consider the root of the test chain a valid EV root for the test policy.
    ScopedTestEVPolicy scoped_test_ev_policy(
        EVRootCAMetadata::GetInstance(),
        X509Certificate::CalculateFingerprint256(root->GetCertBuffer()),
        kEVTestCertPolicy);

    scoped_refptr<X509Certificate> cert = leaf->GetX509CertificateChain();
    ASSERT_TRUE(cert.get());

    scoped_refptr<X509Certificate> intermediate_cert =
        intermediate->GetX509Certificate();
    ASSERT_TRUE(intermediate_cert.get());

    scoped_refptr<X509Certificate> root_cert = root->GetX509Certificate();
    ASSERT_TRUE(root_cert.get());

    if (!trust_the_intermediate) {
      // First trust just the root. This verifies that the test setup is
      // actually correct.
      ScopedTestRoot scoped_test_root({root_cert});
      CertVerifyResult verify_result;
      int flags = 0;
      int error = Verify(cert.get(), "www.example.com", flags, &verify_result);
      EXPECT_THAT(error, IsOk());
      ASSERT_TRUE(verify_result.verified_cert);
      // Verified chain should include the intermediate and the root.
      EXPECT_EQ(2U, verify_result.verified_cert->intermediate_buffers().size());
      // Should be EV.
      EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
    } else {
      // Now try with trusting both the intermediate and the root.
      ScopedTestRoot scoped_test_root({intermediate_cert, root_cert});
      CertVerifyResult verify_result;
      int flags = 0;
      int error = Verify(cert.get(), "www.example.com", flags, &verify_result);
      EXPECT_THAT(error, IsOk());
      ASSERT_TRUE(verify_result.verified_cert);
      // Verified chain should only go to the trusted intermediate, not the
      // root.
      EXPECT_EQ(1U, verify_result.verified_cert->intermediate_buffers().size());
      // Should not be EV.
      EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
    }
  }
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInCommonNameAndNoSAN) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  leaf->EraseExtension(bssl::der::Input(bssl::kSubjectAltNameOid));

  std::string common_name;
  common_name += "www.fake.com";
  common_name += '\0';
  common_name += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectCommonName(common_name);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.fake.com", flags, &verify_result);

  // This actually fails because Chrome only looks for hostnames in
  // SubjectAltNames now and no SubjectAltName is present.
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInCommonNameAndValidSAN) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  leaf->SetSubjectAltName("www.fake.com");

  std::string common_name;
  common_name += "www.fake.com";
  common_name += '\0';
  common_name += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectCommonName(common_name);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.fake.com", flags, &verify_result);

  // SubjectAltName is valid and Chrome does not use the common name.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInSAN) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  std::string hostname;
  hostname += "www.fake.com";
  hostname += '\0';
  hostname += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectAltName(hostname);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.fake.com", flags, &verify_result);

  // SubjectAltName is invalid.
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
}

// Tests the case where the target certificate is accepted by
// X509CertificateBytes, but has errors that should cause verification to fail.
TEST_P(CertVerifyProcInternalTest, InvalidTarget) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> bad_cert;
  if (VerifyProcTypeIsBuiltin()) {
    // Builtin verifier doesn't distinguish between invalid signature algorithm
    // and unknown signature algorithm, so use a different test file that will
    // fail in bssl::ParsedCertificate::Create. The other verifiers use a
    // different test file since the platform verifiers don't all consider empty
    // extensions sequence invalid.
    bad_cert = ImportCertFromFile(certs_dir, "extensions_empty_sequence.pem");
  } else {
    bad_cert = ImportCertFromFile(certs_dir, "signature_algorithm_null.pem");
  }
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(ok_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_with_bad_target(
      X509Certificate::CreateFromBuffer(bssl::UpRef(bad_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_bad_target);
  EXPECT_EQ(1U, cert_with_bad_target->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_with_bad_target.get(), "127.0.0.1", flags, &verify_result);

  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

// Tests the case where an intermediate certificate is accepted by
// X509CertificateBytes, but has errors that should prevent using it during
// verification.  The verification should succeed, since the intermediate
// wasn't necessary.
TEST_P(CertVerifyProcInternalTest, UnnecessaryInvalidIntermediate) {
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));

  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  bssl::UniquePtr<CRYPTO_BUFFER> bad_cert =
      x509_util::CreateCryptoBuffer(std::string_view("invalid"));
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(std::move(bad_cert));
  scoped_refptr<X509Certificate> cert_with_bad_intermediate(
      X509Certificate::CreateFromBuffer(bssl::UpRef(ok_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_bad_intermediate);
  EXPECT_EQ(1U, cert_with_bad_intermediate->intermediate_buffers().size());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  NetLogWithSource net_log(NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert_with_bad_intermediate.get(), "127.0.0.1", flags,
                     &verify_result, net_log);

  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0u, verify_result.cert_status);

  auto events = net_log_observer.GetEntriesForSource(net_log.source());
  EXPECT_FALSE(events.empty());

  auto event = base::ranges::find(events, NetLogEventType::CERT_VERIFY_PROC,
                                  &NetLogEntry::type);
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  const std::string* host = event->params.FindString("host");
  ASSERT_TRUE(host);
  EXPECT_EQ("127.0.0.1", *host);

  if (VerifyProcTypeIsBuiltin()) {
    event =
        base::ranges::find(events, NetLogEventType::CERT_VERIFY_PROC_INPUT_CERT,
                           &NetLogEntry::type);
    ASSERT_NE(event, events.end());
    EXPECT_EQ(net::NetLogEventPhase::NONE, event->phase);
    const std::string* errors = event->params.FindString("errors");
    ASSERT_TRUE(errors);
    EXPECT_EQ(
        "ERROR: Failed parsing Certificate SEQUENCE\nERROR: Failed parsing "
        "Certificate\n",
        *errors);
  }
}

TEST_P(CertVerifyProcInternalTest, RejectExpiredCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(ImportCertFromFile(certs_dir, "root_ca_cert.pem"));

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      certs_dir, "expired_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(0U, cert->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, RejectWeakKeys) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  typedef std::vector<std::string> Strings;
  Strings key_types;

  // These values mush match the prefixes of the key filenames generated by
  // generate-test-keys.sh:
  key_types.push_back("rsa-768");
  key_types.push_back("rsa-1024");
  key_types.push_back("rsa-2048");
  key_types.push_back("ec-prime256v1");

  // Now test each chain.
  for (const std::string& ee_type : key_types) {
    for (const std::string& signer_type : key_types) {
      SCOPED_TRACE("ee_type:" + ee_type + " signer_type:" + signer_type);

      auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

      ASSERT_TRUE(
          leaf->UseKeyFromFile(certs_dir.AppendASCII(ee_type + "-1.key")));
      ASSERT_TRUE(intermediate->UseKeyFromFile(
          certs_dir.AppendASCII(signer_type + "-2.key")));

      ScopedTestRoot scoped_root(root->GetX509Certificate());

      CertVerifyResult verify_result;
      int error = Verify(leaf->GetX509CertificateChain().get(),
                         "www.example.com", 0, &verify_result);

      if (IsInvalidKeyType(ee_type) || IsInvalidKeyType(signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_INVALID,
                  verify_result.cert_status & CERT_STATUS_INVALID);
      } else if (IsWeakKeyType(ee_type) || IsWeakKeyType(signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_WEAK_KEY,
                  verify_result.cert_status & CERT_STATUS_WEAK_KEY);
        EXPECT_EQ(0u, verify_result.cert_status & CERT_STATUS_INVALID);
      } else {
        EXPECT_THAT(error, IsOk());
        EXPECT_EQ(0U, verify_result.cert_status & CERT_STATUS_WEAK_KEY);
      }
    }
  }
}

// Regression test for http://crbug.com/108514.
// Generates a chain with a root with a SHA256 signature, and another root with
// the same name/SPKI/keyid but with a SHA1 signature. The SHA256 root is
// trusted. The SHA1 certificate is supplied as an extra cert, but should be
// ignored as the verifier should prefer the trusted cert when path building
// from the leaf, generating the shortest chain of "leaf -> sha256root". If the
// path builder doesn't prioritize it could build an unoptimal but valid path
// like "leaf -> sha1root -> sha256root".
TEST_P(CertVerifyProcInternalTest, ExtraneousRootCert) {
  auto [leaf_builder, root_builder] = CertBuilder::CreateSimpleChain2();

  root_builder->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha256);
  scoped_refptr<X509Certificate> root_cert = root_builder->GetX509Certificate();

  scoped_refptr<X509Certificate> server_cert =
      leaf_builder->GetX509Certificate();

  // Use the same root_builder but with a new serial number and setting the
  // signature to SHA1, to generate an extraneous self-signed certificate that
  // also signs the leaf cert and which could be used in path-building if the
  // path builder doesn't prioritize trusted roots above other certs.
  root_builder->SetRandomSerialNumber();
  root_builder->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha1);
  scoped_refptr<X509Certificate> extra_cert =
      root_builder->GetX509Certificate();

  ScopedTestRoot scoped_root(root_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(extra_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(server_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);

  CertVerifyResult verify_result;
  int flags = 0;
  int error =
      Verify(cert_chain.get(), "www.example.com", flags, &verify_result);
  EXPECT_THAT(error, IsOk());

  // The extra root should be discarded.
  ASSERT_TRUE(verify_result.verified_cert.get());
  ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(
      verify_result.verified_cert->intermediate_buffers().front().get(),
      root_cert->cert_buffer()));
}

// Test for bug 94673.
TEST_P(CertVerifyProcInternalTest, GoogleDigiNotarTest) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "google_diginotar.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), server_cert.get());

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "diginotar_public_ca_2025.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), intermediate_cert.get());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(server_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);

  CertVerifyResult verify_result;
  int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  int error =
      Verify(cert_chain.get(), "mail.google.com", flags, &verify_result);
  EXPECT_NE(OK, error);

  // Now turn off revocation checking.  Certificate verification should still
  // fail.
  flags = 0;
  error = Verify(cert_chain.get(), "mail.google.com", flags, &verify_result);
  EXPECT_NE(OK, error);
}

TEST_P(CertVerifyProcInternalTest, NameConstraintsOk) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Use the private key matching the public_key_hash of the kDomainsTest
  // constraint in CertVerifyProc::HasNameConstraintsViolation.
  ASSERT_TRUE(leaf->UseKeyFromFile(
      GetTestCertsDirectory().AppendASCII("name_constrained_key.pem")));
  // example.com is allowed by kDomainsTest, and notarealtld is not a known
  // TLD, so that's allowed too.
  leaf->SetSubjectAltNames({"test.ExAmPlE.CoM", "example.notarealtld",
                            "*.test2.ExAmPlE.CoM", "*.example2.notarealtld"},
                           {});

  ScopedTestRoot test_root(root->GetX509Certificate());

  scoped_refptr<X509Certificate> leaf_cert = leaf->GetX509Certificate();

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf_cert.get(), "test.example.com", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  error =
      Verify(leaf_cert.get(), "foo.test2.example.com", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
}

TEST_P(CertVerifyProcInternalTest, NameConstraintsFailure) {
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  // Use the private key matching the public_key_hash of the kDomainsTest
  // constraint in CertVerifyProc::HasNameConstraintsViolation.
  ASSERT_TRUE(leaf->UseKeyFromFile(
      GetTestCertsDirectory().AppendASCII("name_constrained_key.pem")));
  // example.com is allowed by kDomainsTest, but example.org is not.
  leaf->SetSubjectAltNames({"test.ExAmPlE.CoM", "test.ExAmPlE.OrG"}, {});

  ScopedTestRoot test_root(root->GetX509Certificate());

  scoped_refptr<X509Certificate> leaf_cert = leaf->GetX509Certificate();

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf_cert.get(), "test.example.com", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_NAME_CONSTRAINT_VIOLATION));
  EXPECT_EQ(CERT_STATUS_NAME_CONSTRAINT_VIOLATION,
            verify_result.cert_status & CERT_STATUS_NAME_CONSTRAINT_VIOLATION);
}

// This fixture is for testing the verification of a certificate chain which
// has some sort of mismatched signature algorithm (i.e.
// Certificate.signatureAlgorithm and TBSCertificate.algorithm are different).
class CertVerifyProcInspectSignatureAlgorithmsTest : public ::testing::Test {
 protected:
  // In the test setup, SHA384 is given special treatment as an unknown
  // algorithm.
  static constexpr bssl::DigestAlgorithm kUnknownDigestAlgorithm =
      bssl::DigestAlgorithm::Sha384;

  struct CertParams {
    // Certificate.signatureAlgorithm
    bssl::DigestAlgorithm cert_algorithm;

    // TBSCertificate.algorithm
    bssl::DigestAlgorithm tbs_algorithm;
  };

  // Shorthand for VerifyChain() where only the leaf's parameters need
  // to be specified.
  [[nodiscard]] int VerifyLeaf(const CertParams& leaf_params) {
    return VerifyChain(
        {// Target
         leaf_params,
         // Root
         {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256}});
  }

  // Shorthand for VerifyChain() where only the intermediate's parameters need
  // to be specified.
  [[nodiscard]] int VerifyIntermediate(const CertParams& intermediate_params) {
    return VerifyChain(
        {// Target
         {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256},
         // Intermediate
         intermediate_params,
         // Root
         {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256}});
  }

  // Shorthand for VerifyChain() where only the root's parameters need to be
  // specified.
  [[nodiscard]] int VerifyRoot(const CertParams& root_params) {
    return VerifyChain(
        {// Target
         {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256},
         // Intermediate
         {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256},
         // Root
         root_params});
  }

  // Manufactures a certificate chain where each certificate has the indicated
  // signature algorithms, and then returns the result of verifying this chain.
  [[nodiscard]] int VerifyChain(const std::vector<CertParams>& chain_params) {
    // Manufacture a chain with the given combinations of signature algorithms.
    // This chain isn't actually a valid chain, but it is good enough for
    // testing the base CertVerifyProc.
    std::vector<std::unique_ptr<CertBuilder>> builders =
        CertBuilder::CreateSimpleChain(chain_params.size());
    for (size_t i = 0; i < chain_params.size(); i++) {
      builders[i]->SetOuterSignatureAlgorithmTLV(base::as_string_view(
          GetAlgorithmSequence(chain_params[i].cert_algorithm)));
      builders[i]->SetTBSSignatureAlgorithmTLV(base::as_string_view(
          GetAlgorithmSequence(chain_params[i].tbs_algorithm)));
    }

    scoped_refptr<X509Certificate> chain =
        builders.front()->GetX509CertificateFullChain();
    if (!chain) {
      ADD_FAILURE() << "Failed creating certificate chain";
      return ERR_UNEXPECTED;
    }

    int flags = 0;
    CertVerifyResult dummy_result;
    CertVerifyResult verify_result;

    auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);

    return verify_proc->Verify(
        chain.get(), "www.example.com", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  }

 private:
  static base::span<const uint8_t> GetAlgorithmSequence(
      bssl::DigestAlgorithm algorithm) {
    switch (algorithm) {
      case bssl::DigestAlgorithm::Sha1:
        static const uint8_t kSha1WithRSAEncryption[] = {
            0x30, 0x0D, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
            0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00};
        return kSha1WithRSAEncryption;
      case bssl::DigestAlgorithm::Sha256:
        static const uint8_t kSha256WithRSAEncryption[] = {
            0x30, 0x0D, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
            0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00};
        return kSha256WithRSAEncryption;
      case kUnknownDigestAlgorithm:
        static const uint8_t kUnknownAlgorithm[] = {
            0x30, 0x0D, 0x06, 0x09, 0x8a, 0x87, 0x18, 0x46,
            0xd7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00};
        return kUnknownAlgorithm;
      default:
        NOTREACHED() << "Unsupported digest algorithm";
    }
  }
};

// This is a control test to make sure that the test helper
// VerifyLeaf() works as expected. There is no actual mismatch in the
// algorithms used here.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha1WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha1Sha1) {
  int rv =
      VerifyLeaf({bssl::DigestAlgorithm::Sha1, bssl::DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
}

// This is a control test to make sure that the test helper
// VerifyLeaf() works as expected. There is no actual mismatch in the
// algorithms used here.
//
//  Certificate.signatureAlgorithm:  sha256WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Sha256) {
  int rv = VerifyLeaf(
      {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsOk());
}

// Mismatched signature algorithms in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha1Sha256) {
  int rv =
      VerifyLeaf({bssl::DigestAlgorithm::Sha1, bssl::DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Sha1) {
  int rv =
      VerifyLeaf({bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Unrecognized signature algorithm in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        ?
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Unknown) {
  int rv = VerifyLeaf({bssl::DigestAlgorithm::Sha256, kUnknownDigestAlgorithm});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Unrecognized signature algorithm in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  ?
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafUnknownSha256) {
  int rv = VerifyLeaf({kUnknownDigestAlgorithm, bssl::DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the intermediate certificate.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, IntermediateSha1Sha256) {
  int rv = VerifyIntermediate(
      {bssl::DigestAlgorithm::Sha1, bssl::DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the intermediate certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, IntermediateSha256Sha1) {
  int rv = VerifyIntermediate(
      {bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the root certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, RootSha256Sha1) {
  int rv =
      VerifyRoot({bssl::DigestAlgorithm::Sha256, bssl::DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsOk());
}

// Unrecognized signature algorithm in the root certificate.
//
//  Certificate.signatureAlgorithm:  ?
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, RootUnknownSha256) {
  int rv = VerifyRoot({kUnknownDigestAlgorithm, bssl::DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsOk());
}

TEST(CertVerifyProcTest, TestHasTooLongValidity) {
  struct {
    const char* const file;
    bool is_valid_too_long;
  } tests[] = {
      {"start_after_expiry.pem", true},
      {"pre_br_validity_ok.pem", true},
      {"pre_br_validity_bad_121.pem", true},
      {"pre_br_validity_bad_2020.pem", true},
      {"10_year_validity.pem", true},
      {"11_year_validity.pem", true},
      {"39_months_after_2015_04.pem", true},
      {"40_months_after_2015_04.pem", true},
      {"60_months_after_2012_07.pem", true},
      {"61_months_after_2012_07.pem", true},
      {"825_days_after_2018_03_01.pem", true},
      {"826_days_after_2018_03_01.pem", true},
      {"825_days_1_second_after_2018_03_01.pem", true},
      {"39_months_based_on_last_day.pem", true},
      {"398_days_after_2020_09_01.pem", false},
      {"399_days_after_2020_09_01.pem", true},
      {"398_days_1_second_after_2020_09_01.pem", true},
  };

  base::FilePath certs_dir = GetTestCertsDirectory();

  for (const auto& test : tests) {
    SCOPED_TRACE(test.file);

    scoped_refptr<X509Certificate> certificate =
        ImportCertFromFile(certs_dir, test.file);
    ASSERT_TRUE(certificate);
    EXPECT_EQ(test.is_valid_too_long,
              CertVerifyProc::HasTooLongValidity(*certificate));
  }
}

// Integration test for CertVerifyProc::HasTooLongValidity.
// There isn't a way to add test entries to the known roots list for testing
// the full CertVerifyProc implementations, but HasTooLongValidity is checked
// by the outer CertVerifyProc::Verify. Thus the test can mock the
// VerifyInternal result to pretend there was a successful verification with
// is_issued_by_known_root and see that Verify overrides that with error.
TEST(CertVerifyProcTest, VerifyCertValidityTooLong) {
  scoped_refptr<X509Certificate> cert(ImportCertFromFile(
      GetTestCertsDirectory(), "900_days_after_2019_07_01.pem"));
  ASSERT_TRUE(cert);

  {
    // Locally trusted cert should be ok.
    CertVerifyResult dummy_result;
    dummy_result.is_issued_by_known_root = false;
    auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);
    CertVerifyResult verify_result;
    int error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &verify_result, NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_EQ(0u, verify_result.cert_status & CERT_STATUS_ALL_ERRORS);
  }

  {
    // Publicly trusted cert that was otherwise okay should get changed to
    // ERR_CERT_VALIDITY_TOO_LONG.
    CertVerifyResult dummy_result;
    dummy_result.is_issued_by_known_root = true;
    auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);
    CertVerifyResult verify_result;
    int error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &verify_result, NetLogWithSource());
    EXPECT_THAT(error, IsError(ERR_CERT_VALIDITY_TOO_LONG));
    // TODO(mattm): generate a dedicated cert or use CertBuilder so that this
    // test doesn't also hit CERT_STATUS_NON_UNIQUE_NAME.
    EXPECT_EQ(CERT_STATUS_VALIDITY_TOO_LONG | CERT_STATUS_NON_UNIQUE_NAME,
              verify_result.cert_status & CERT_STATUS_ALL_ERRORS);
  }

  {
    // Publicly trusted cert that had some other error should retain the
    // original error, but CERT_STATUS_VALIDITY_TOO_LONG should be added to
    // cert_status.
    CertVerifyResult dummy_result;
    dummy_result.is_issued_by_known_root = true;
    dummy_result.cert_status = CERT_STATUS_AUTHORITY_INVALID;
    auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(
        dummy_result, ERR_CERT_AUTHORITY_INVALID);
    CertVerifyResult verify_result;
    int error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &verify_result, NetLogWithSource());
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    // TODO(mattm): generate a dedicated cert or use CertBuilder so that this
    // test doesn't also hit CERT_STATUS_NON_UNIQUE_NAME.
    EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID | CERT_STATUS_VALIDITY_TOO_LONG |
                  CERT_STATUS_NON_UNIQUE_NAME,
              verify_result.cert_status & CERT_STATUS_ALL_ERRORS);
  }
}

TEST_P(CertVerifyProcInternalTest, TestKnownRoot) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "leaf_from_known_root.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_chain.get(), "foo.chickentools.org", flags, &verify_result);
  EXPECT_THAT(error, IsOk())
      << "This test relies on a real certificate that "
      << "expires on May 11 2025. If failing on/after "
      << "that date, please disable and file a bug "
      << "against mattm. Current time: " << base::Time::Now();
  EXPECT_TRUE(verify_result.is_issued_by_known_root);
}

// This tests that on successful certificate verification,
// CertVerifyResult::public_key_hashes is filled with a SHA256 hash for each
// of the certificates in the chain.
TEST_P(CertVerifyProcInternalTest, PublicKeyHashes) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2]);
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert_chain.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());

  EXPECT_EQ(3u, verify_result.public_key_hashes.size());

  // Convert |public_key_hashes| to strings for ease of comparison.
  std::vector<std::string> public_key_hash_strings;
  for (const auto& public_key_hash : verify_result.public_key_hashes)
    public_key_hash_strings.push_back(public_key_hash.ToString());

  std::vector<std::string> expected_public_key_hashes = {
      // Target
      "sha256/Ru/08Ru275Zlf42sbI6lqi2OUun3r4YgrrK/vJ3+Yzk=",

      // Intermediate
      "sha256/D9u0epgvPYlG9YiVp7V+IMT+xhUpB5BhsS/INjDXc4Y=",

      // Trust anchor
      "sha256/VypP3VWL7OaqTJ7mIBehWYlv8khPuFHpWiearZI2YjI="};

  // |public_key_hashes| does not have an ordering guarantee.
  EXPECT_THAT(expected_public_key_hashes,
              testing::UnorderedElementsAreArray(public_key_hash_strings));
}

// Basic test for returning the chain in CertVerifyResult. Note that the
// returned chain may just be a reflection of the originally supplied chain;
// that is, if any errors occur, the default chain returned is an exact copy
// of the certificate to be verified. The remaining VerifyReturn* tests are
// used to ensure that the actual, verified chain is being returned by
// Verify().
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainBasic) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), google_full_chain.get());
  ASSERT_EQ(2U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(nullptr),
            verify_result.verified_cert.get());
  int error = Verify(google_full_chain.get(), "127.0.0.1", 0, &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            verify_result.verified_cert.get());

  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

// Test that certificates issued for 'intranet' names (that is, containing no
// known public registry controlled domain information) issued by well-known
// CAs are flagged appropriately, while certificates that are issued by
// internal CAs are not flagged.
TEST(CertVerifyProcTest, IntranetHostsRejected) {
  const std::string kIntranetHostname = "webmail";

  auto [leaf, root] = CertBuilder::CreateSimpleChain2();
  leaf->SetSubjectAltName(kIntranetHostname);

  scoped_refptr<X509Certificate> cert(leaf->GetX509Certificate());

  CertVerifyResult verify_result;
  int error = 0;

  // Intranet names for public CAs should be flagged:
  CertVerifyResult dummy_result;
  dummy_result.is_issued_by_known_root = true;
  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);
  error = verify_proc->Verify(cert.get(), kIntranetHostname,
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), 0, &verify_result,
                              NetLogWithSource());
  // Intranet certificates from known roots are accepted without error in Cronet
  // to avoid breaking consumer tests. See b/337196170 (Google-internal).
#if BUILDFLAG(CRONET_BUILD)
  EXPECT_THAT(error, IsOk());
#else
  EXPECT_THAT(error, IsError(ERR_CERT_NON_UNIQUE_NAME));
#endif  // BUILDFLAG(CRONET_BUILD)
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);

  // However, if the CA is not well known, these should not be flagged:
  dummy_result.Reset();
  dummy_result.is_issued_by_known_root = false;
  verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);
  error = verify_proc->Verify(cert.get(), kIntranetHostname,
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), 0, &verify_result,
                              NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);
}

// Tests that certificates issued by Symantec's legacy infrastructure
// are rejected according to the policies outlined in
// https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
// unless the caller has explicitly disabled that enforcement.
TEST(CertVerifyProcTest, SymantecCertsRejected) {
  constexpr SHA256HashValue kSymantecHashValue = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  constexpr SHA256HashValue kGoogleHashValue = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  static constexpr base::Time may_1_2016 = base::Time::FromTimeT(1462060800);
  leaf->SetValidity(may_1_2016, may_1_2016 + base::Days(1));
  scoped_refptr<X509Certificate> leaf_pre_june_2016 =
      leaf->GetX509Certificate();

  static constexpr base::Time june_1_2016 = base::Time::FromTimeT(1464739200);
  leaf->SetValidity(june_1_2016, june_1_2016 + base::Days(1));
  scoped_refptr<X509Certificate> leaf_post_june_2016 =
      leaf->GetX509Certificate();

  static constexpr base::Time dec_20_2017 = base::Time::FromTimeT(1513728000);
  leaf->SetValidity(dec_20_2017, dec_20_2017 + base::Days(1));
  scoped_refptr<X509Certificate> leaf_dec_2017 = leaf->GetX509Certificate();

  // Test that certificates from the legacy Symantec infrastructure are
  // rejected:
  // leaf_dec_2017: A certificate issued after 2017-12-01, which is rejected
  //                as of M65
  // leaf_pre_june_2016: A certificate issued prior to 2016-06-01, which is
  //                     rejected as of M66.
  for (X509Certificate* cert :
       {leaf_dec_2017.get(), leaf_pre_june_2016.get()}) {
    scoped_refptr<CertVerifyProc> verify_proc;
    int error = 0;

    // Test that a legacy Symantec certificate is rejected.
    CertVerifyResult symantec_result;
    symantec_result.verified_cert = cert;
    symantec_result.public_key_hashes.push_back(HashValue(kSymantecHashValue));
    symantec_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(symantec_result);

    CertVerifyResult test_result_1;
    error = verify_proc->Verify(
        cert, "www.example.com", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &test_result_1, NetLogWithSource());
    EXPECT_THAT(error, IsError(ERR_CERT_SYMANTEC_LEGACY));
    EXPECT_TRUE(test_result_1.cert_status & CERT_STATUS_SYMANTEC_LEGACY);

    // ... Unless the Symantec cert chains through a allowlisted intermediate.
    CertVerifyResult allowlisted_result;
    allowlisted_result.verified_cert = cert;
    allowlisted_result.public_key_hashes.push_back(
        HashValue(kSymantecHashValue));
    allowlisted_result.public_key_hashes.push_back(HashValue(kGoogleHashValue));
    allowlisted_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(allowlisted_result);

    CertVerifyResult test_result_2;
    error = verify_proc->Verify(
        cert, "www.example.com", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &test_result_2, NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_2.cert_status & CERT_STATUS_AUTHORITY_INVALID);

    // ... Or the caller disabled enforcement of Symantec policies.
    CertVerifyResult test_result_3;
    error = verify_proc->Verify(
        cert, "www.example.com", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(),
        CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT, &test_result_3,
        NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_3.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
  }

  // Test that certificates from the legacy Symantec infrastructure issued
  // after 2016-06-01 appropriately rejected.
  scoped_refptr<X509Certificate> cert = leaf_post_june_2016;

  scoped_refptr<CertVerifyProc> verify_proc;
  int error = 0;

  // Test that a legacy Symantec certificate is rejected if the feature
  // flag is enabled, and accepted if it is not.
  CertVerifyResult symantec_result;
  symantec_result.verified_cert = cert;
  symantec_result.public_key_hashes.push_back(HashValue(kSymantecHashValue));
  symantec_result.is_issued_by_known_root = true;
  verify_proc = base::MakeRefCounted<MockCertVerifyProc>(symantec_result);

  CertVerifyResult test_result_1;
  error = verify_proc->Verify(cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), 0, &test_result_1,
                              NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_SYMANTEC_LEGACY));
  EXPECT_TRUE(test_result_1.cert_status & CERT_STATUS_SYMANTEC_LEGACY);

  // ... Unless the Symantec cert chains through a allowlisted intermediate.
  CertVerifyResult allowlisted_result;
  allowlisted_result.verified_cert = cert;
  allowlisted_result.public_key_hashes.push_back(HashValue(kSymantecHashValue));
  allowlisted_result.public_key_hashes.push_back(HashValue(kGoogleHashValue));
  allowlisted_result.is_issued_by_known_root = true;
  verify_proc = base::MakeRefCounted<MockCertVerifyProc>(allowlisted_result);

  CertVerifyResult test_result_2;
  error = verify_proc->Verify(cert.get(), "www.example.com",
                              /*ocsp_response=*/std::string(),
                              /*sct_list=*/std::string(), 0, &test_result_2,
                              NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(test_result_2.cert_status & CERT_STATUS_AUTHORITY_INVALID);

  // ... Or the caller disabled enforcement of Symantec policies.
  CertVerifyResult test_result_3;
  error = verify_proc->Verify(
      cert.get(), "www.example.com", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(),
      CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT, &test_result_3,
      NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(test_result_3.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
}

// Test that the certificate returned in CertVerifyResult is able to reorder
// certificates that are not ordered from end-entity to root. While this is
// a protocol violation if sent during a TLS handshake, if multiple sources
// of intermediate certificates are combined, it's possible that order may
// not be maintained.
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainProperlyOrdered) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  // Construct the chain out of order.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(google_full_chain);
  ASSERT_EQ(2U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_FALSE(verify_result.verified_cert);
  int error = Verify(google_full_chain.get(), "127.0.0.1", 0, &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

// Test that Verify() filters out certificates which are not related to
// or part of the certificate chain being verified.
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainFiltersUnrelatedCerts) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());
  ScopedTestRoot scoped_root(certs[2]);

  scoped_refptr<X509Certificate> unrelated_certificate =
      ImportCertFromFile(certs_dir, "duplicate_cn_1.pem");
  scoped_refptr<X509Certificate> unrelated_certificate2 =
      ImportCertFromFile(certs_dir, "google.single.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            unrelated_certificate.get());
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            unrelated_certificate2.get());

  // Interject unrelated certificates into the list of intermediates.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(unrelated_certificate->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(unrelated_certificate2->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(google_full_chain);
  ASSERT_EQ(4U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_FALSE(verify_result.verified_cert);
  int error = Verify(google_full_chain.get(), "127.0.0.1", 0, &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

TEST_P(CertVerifyProcInternalTest, AdditionalTrustAnchors) {
  if (!VerifyProcTypeIsBuiltin()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  // |ca_cert| is the issuer of |cert|.
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  scoped_refptr<X509Certificate> ca_cert(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  // Verification of |cert| fails when |ca_cert| is not in the trust anchors
  // list.
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);

  // Now add the |ca_cert| to the |trust_anchors|, and verification should pass.
  CertificateList trust_anchors;
  trust_anchors.push_back(ca_cert);
  SetUpWithAdditionalCerts(trust_anchors, {});
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_additional_trust_anchor);

  // Clearing the |trust_anchors| makes verification fail again (the cache
  // should be skipped).
  SetUpWithAdditionalCerts({}, {});
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);
}

TEST_P(CertVerifyProcInternalTest, AdditionalIntermediates) {
  if (!VerifyProcTypeIsBuiltin()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  scoped_refptr<X509Certificate> leaf_cert = leaf->GetX509Certificate();
  scoped_refptr<X509Certificate> intermediate_cert =
      intermediate->GetX509Certificate();
  scoped_refptr<X509Certificate> root_cert = root->GetX509Certificate();
  constexpr char kHostname[] = "www.example.com";

  ScopedTestRoot trust_root(root_cert);
  // Leaf should not verify without intermediate found
  EXPECT_THAT(Verify(leaf_cert.get(), kHostname),
              IsError(ERR_CERT_AUTHORITY_INVALID));

  // Leaf should verify after intermediate is passed in to CertVerifyProc. Chain
  // should be {leaf, intermediate, root}.
  SetUpWithAdditionalCerts({}, {intermediate->GetX509Certificate()});
  CertVerifyResult verify_result;
  int error = Verify(leaf_cert.get(), kHostname, /*flags=*/0, &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);
  EXPECT_EQ(verify_result.verified_cert->intermediate_buffers().size(), 2U);
  EXPECT_TRUE(x509_util::CryptoBufferEqual(
      verify_result.verified_cert->intermediate_buffers().back().get(),
      root_cert->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(
      verify_result.verified_cert->intermediate_buffers().front().get(),
      intermediate_cert->cert_buffer()));
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);
}

TEST_P(CertVerifyProcInternalTest, AdditionalIntermediateDuplicatesRoot) {
  if (!VerifyProcTypeIsBuiltin()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  scoped_refptr<X509Certificate> leaf_cert = leaf->GetX509Certificate();
  scoped_refptr<X509Certificate> intermediate_cert =
      intermediate->GetX509Certificate();
  scoped_refptr<X509Certificate> root_cert = root->GetX509Certificate();
  constexpr char kHostname[] = "www.example.com";

  // The root is trusted through ScopedTestRoot, not through
  // additional_trust_anchors.
  ScopedTestRoot trust_root(root_cert);
  // In addition to the intermediate cert, the root cert is also configured as
  // an additional *untrusted* certificate, which is harmless. This shouldn't
  // cause the result to be considered as is_issued_by_additional_trust_anchor.
  SetUpWithAdditionalCerts(
      {}, {root->GetX509Certificate(), intermediate->GetX509Certificate()});
  CertVerifyResult verify_result;
  int error = Verify(leaf_cert.get(), kHostname, /*flags=*/0, &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);
  EXPECT_EQ(verify_result.verified_cert->intermediate_buffers().size(), 2U);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);
}

TEST_P(CertVerifyProcInternalTest, AdditionalTrustAnchorDuplicateIntermediate) {
  if (!VerifyProcTypeIsBuiltin()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  constexpr char kHostname[] = "www.example.com";

  // Leaf should not verify without anything set up.
  EXPECT_THAT(Verify(leaf->GetX509Certificate().get(), kHostname),
              IsError(ERR_CERT_AUTHORITY_INVALID));

  // Leaf should verify with intermediate and root added.
  CertificateList trust_anchors, intermediates;
  intermediates.push_back(intermediate->GetX509Certificate());
  trust_anchors.push_back(root->GetX509Certificate());
  SetUpWithAdditionalCerts(trust_anchors, intermediates);
  CertVerifyResult verify_result;
  EXPECT_THAT(Verify(leaf->GetX509Certificate().get(), kHostname,
                     /*flags=*/0, &verify_result),
              IsOk());
  EXPECT_TRUE(verify_result.is_issued_by_additional_trust_anchor);

  // Leaf should still verify after root is also in intermediates list.
  intermediates.push_back(root->GetX509Certificate());
  SetUpWithAdditionalCerts(trust_anchors, intermediates);
  EXPECT_THAT(Verify(leaf->GetX509Certificate().get(), kHostname,
                     /*flags=*/0, &verify_result),
              IsOk());
  EXPECT_TRUE(verify_result.is_issued_by_additional_trust_anchor);
}

// Tests that certificates issued by user-supplied roots are not flagged as
// issued by a known root. This should pass whether or not the platform supports
// detecting known roots.
TEST_P(CertVerifyProcInternalTest, IsIssuedByKnownRootIgnoresTestRoots) {
  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));

  // Verification should pass.
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
  // But should not be marked as a known root.
  EXPECT_FALSE(verify_result.is_issued_by_known_root);
}

// Test that CRLSets are effective in making a certificate appear to be
// revoked.
TEST_P(CertVerifyProcInternalTest, CRLSet) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;

  // First test blocking by SPKI.
  EXPECT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Second, test revocation by serial number of a cert directly under the
  // root.
  crl_set_bytes.clear();
  EXPECT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_serial.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
}

TEST_P(CertVerifyProcInternalTest, CRLSetLeafSerial) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  scoped_refptr<X509Certificate> leaf = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(leaf);
  ASSERT_EQ(1U, leaf->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());

  // Test revocation by serial number of a certificate not under the root.
  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_intermediate_serial.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
}

TEST_P(CertVerifyProcInternalTest, CRLSetRootReturnsChain) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0]);

  scoped_refptr<X509Certificate> leaf = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(leaf);
  ASSERT_EQ(1U, leaf->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());

  // Test revocation of the root itself.
  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  EXPECT_EQ(3u, verify_result.public_key_hashes.size());
  ASSERT_TRUE(verify_result.verified_cert);
  EXPECT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
}

// Tests that CertVerifyProc implementations apply CRLSet revocations by
// subject.
TEST_P(CertVerifyProcInternalTest, CRLSetRevokedBySubject) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  scoped_refptr<X509Certificate> root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(root);

  scoped_refptr<X509Certificate> leaf(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(leaf);

  ScopedTestRoot scoped_root(root);

  int flags = 0;
  CertVerifyResult verify_result;

  // Confirm that verifying the certificate chain with an empty CRLSet succeeds.
  SetUpCertVerifyProc(CRLSet::EmptyCRLSetForTesting());
  int error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());

  std::string crl_set_bytes;
  scoped_refptr<CRLSet> crl_set;

  // Revoke the leaf by subject. Verification should now fail.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_leaf_subject_no_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Revoke the root by subject. Verification should now fail.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_subject_no_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Revoke the leaf by subject, but only if the SPKI doesn't match the given
  // one. Verification should pass when using the certificate's actual SPKI.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_subject.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(leaf.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
}

// Ensures that CRLSets can be used to block known interception roots on
// platforms that support CRLSets, while otherwise detect known interception
// on platforms that do not.
TEST_P(CertVerifyProcInternalTest, BlockedInterceptionByRoot) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root);

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSet should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  SetUpCertVerifyProc(crl_set);
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the root as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestCertsDirectory().AppendASCII(
                                 "crlset_blocked_interception_by_root.raw"),
                             &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  if (SupportsCRLSet()) {
    EXPECT_THAT(error, IsError(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED));
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  } else {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
  }
}

// Ensures that CRLSets can be used to block known interception intermediates,
// while still allowing other certificates from that root..
TEST_P(CertVerifyProcInternalTest, BlockedInterceptionByIntermediate) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root);

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSEt should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  SetUpCertVerifyProc(crl_set);
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the intermediate as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII(
          "crlset_blocked_interception_by_intermediate.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  if (SupportsCRLSet()) {
    EXPECT_THAT(error, IsError(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED));
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  } else {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
  }

  // Load a different certificate from that root, which should be unaffected.
  scoped_refptr<X509Certificate> second_cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(second_cert);

  SetUpCertVerifyProc(crl_set);
  error = Verify(second_cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
}

// Ensures that CRLSets can be used to flag known interception roots, even
// when they are not blocked.
TEST_P(CertVerifyProcInternalTest, DetectsInterceptionByRoot) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root);

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSet should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  SetUpCertVerifyProc(crl_set);
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the root as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestCertsDirectory().AppendASCII(
                                 "crlset_known_interception_by_root.raw"),
                             &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  SetUpCertVerifyProc(crl_set);
  error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status &
              CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
}

// Tests that CRLSets participate in path building functions, and that as
// long as a valid path exists within the verification graph, verification
// succeeds.
//
// In this test, there are two roots (D and E), and three possible paths
// to validate a leaf (A):
// 1. A(B) -> B(C) -> C(D) -> D(D)
// 2. A(B) -> B(C) -> C(E) -> E(E)
// 3. A(B) -> B(F) -> F(E) -> E(E)
//
// Each permutation of revocation is tried:
// 1. Revoking E by SPKI, so that only Path 1 is valid (as E is in Paths 2 & 3)
// 2. Revoking C(D) and F(E) by serial, so that only Path 2 is valid.
// 3. Revoking C by SPKI, so that only Path 3 is valid (as C is in Paths 1 & 2)
TEST_P(CertVerifyProcInternalTest, CRLSetDuringPathBuilding) {
  if (!SupportsCRLSetsInPathBuilding()) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  CertificateList path_1_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-C.pem",
                            "multi-root-C-by-D.pem", "multi-root-D-by-D.pem"},
                           &path_1_certs));

  CertificateList path_2_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-C.pem",
                            "multi-root-C-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_2_certs));

  CertificateList path_3_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-F.pem",
                            "multi-root-F-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_3_certs));

  // Add D and E as trust anchors.
  ScopedTestRoot test_root_D(path_1_certs[3]);  // D-by-D
  ScopedTestRoot test_root_E(path_2_certs[3]);  // E-by-E

  // Create a chain that contains all the certificate paths possible.
  // CertVerifyProcInternalTest.VerifyReturnChainFiltersUnrelatedCerts already
  // ensures that it's safe to send additional certificates as inputs, and
  // that they're ignored if not necessary.
  // This is to avoid relying on AIA or internal object caches when
  // interacting with the underlying library.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      bssl::UpRef(path_1_certs[1]->cert_buffer()));  // B-by-C
  intermediates.push_back(
      bssl::UpRef(path_1_certs[2]->cert_buffer()));  // C-by-D
  intermediates.push_back(
      bssl::UpRef(path_2_certs[2]->cert_buffer()));  // C-by-E
  intermediates.push_back(
      bssl::UpRef(path_3_certs[1]->cert_buffer()));  // B-by-F
  intermediates.push_back(
      bssl::UpRef(path_3_certs[2]->cert_buffer()));  // F-by-E
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(path_1_certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert);

  struct TestPermutations {
    const char* crlset;
    bool expect_valid;
    scoped_refptr<X509Certificate> expected_intermediate;
  } kTests[] = {
      {"multi-root-crlset-D-and-E.raw", false, nullptr},
      {"multi-root-crlset-E.raw", true, path_1_certs[2].get()},
      {"multi-root-crlset-CD-and-FE.raw", true, path_2_certs[2].get()},
      {"multi-root-crlset-C.raw", true, path_3_certs[2].get()},
      {"multi-root-crlset-unrelated.raw", true, nullptr}};

  for (const auto& testcase : kTests) {
    SCOPED_TRACE(testcase.crlset);
    scoped_refptr<CRLSet> crl_set;
    std::string crl_set_bytes;
    EXPECT_TRUE(base::ReadFileToString(
        GetTestCertsDirectory().AppendASCII(testcase.crlset), &crl_set_bytes));
    ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

    SetUpCertVerifyProc(crl_set);
    int flags = 0;
    CertVerifyResult verify_result;
    int error = Verify(cert.get(), "127.0.0.1", flags, &verify_result);

    if (!testcase.expect_valid) {
      EXPECT_NE(OK, error);
      EXPECT_NE(0U, verify_result.cert_status);
      continue;
    }

    ASSERT_THAT(error, IsOk());
    ASSERT_EQ(0U, verify_result.cert_status);
    ASSERT_TRUE(verify_result.verified_cert.get());

    if (!testcase.expected_intermediate)
      continue;

    const auto& verified_intermediates =
        verify_result.verified_cert->intermediate_buffers();
    ASSERT_EQ(3U, verified_intermediates.size());

    scoped_refptr<X509Certificate> intermediate =
        X509Certificate::CreateFromBuffer(
            bssl::UpRef(verified_intermediates[1].get()), {});
    ASSERT_TRUE(intermediate);

    EXPECT_TRUE(testcase.expected_intermediate->EqualsExcludingChain(
        intermediate.get()))
        << "Expected: " << testcase.expected_intermediate->subject().common_name
        << " issued by " << testcase.expected_intermediate->issuer().common_name
        << "; Got: " << intermediate->subject().common_name << " issued by "
        << intermediate->issuer().common_name;
  }
}

TEST_P(CertVerifyProcInternalTest, ValidityDayPlus5MinutesBeforeNotBefore) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() + base::Days(1) + base::Minutes(5);
  base::Time not_after = base::Time::Now() + base::Days(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityDayBeforeNotBefore) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() + base::Days(1);
  base::Time not_after = base::Time::Now() + base::Days(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityJustBeforeNotBefore) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() + base::Minutes(5);
  base::Time not_after = base::Time::Now() + base::Days(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityJustAfterNotBefore) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() - base::Seconds(1);
  base::Time not_after = base::Time::Now() + base::Days(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is between notBefore and notAfter. Verification should
  // succeed.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, ValidityJustBeforeNotAfter) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() - base::Days(30);
  base::Time not_after = base::Time::Now() + base::Minutes(5);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is between notBefore and notAfter. Verification should
  // succeed.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, ValidityJustAfterNotAfter) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  base::Time not_before = base::Time::Now() - base::Days(30);
  base::Time not_after = base::Time::Now() - base::Seconds(1);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), "www.example.com", flags, &verify_result);
  // Current time is after certificate's notAfter. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, FailedIntermediateSignatureValidation) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Intermediate has no authorityKeyIdentifier. Also remove
  // subjectKeyIdentifier from root for good measure.
  intermediate->EraseExtension(
      bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
  root->EraseExtension(bssl::der::Input(bssl::kSubjectKeyIdentifierOid));

  // Get the chain with the leaf and the intermediate signed by the original
  // key of |root|.
  scoped_refptr<X509Certificate> cert = leaf->GetX509CertificateChain();

  // Generate a new key for root.
  root->GenerateECKey();

  // Trust the new root certificate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "www.example.com", flags, &verify_result);

  // The intermediate was signed by a different root with a different key but
  // with the same name as the trusted one, and the intermediate has no
  // authorityKeyIdentifier, so the verifier must try verifying the signature.
  // Should fail with AUTHORITY_INVALID.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcInternalTest, FailedTargetSignatureValidation) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Leaf has no authorityKeyIdentifier. Also remove subjectKeyIdentifier from
  // intermediate for good measure.
  leaf->EraseExtension(bssl::der::Input(bssl::kAuthorityKeyIdentifierOid));
  intermediate->EraseExtension(
      bssl::der::Input(bssl::kSubjectKeyIdentifierOid));

  // Get a copy of the leaf signed by the original key of intermediate.
  bssl::UniquePtr<CRYPTO_BUFFER> leaf_wrong_signature = leaf->DupCertBuffer();

  // Generate a new key for intermediate.
  intermediate->GenerateECKey();

  // Make a chain that includes the original leaf with the wrong signature and
  // the new intermediate.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(intermediate->DupCertBuffer());

  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(leaf_wrong_signature), std::move(intermediates));
  ASSERT_TRUE(cert.get());

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "www.example.com", flags, &verify_result);

  // The leaf was signed by a different intermediate with a different key but
  // with the same name as the one in the chain, and the leaf has no
  // authorityKeyIdentifier, so the verifier must try verifying the signature.
  // Should fail with AUTHORITY_INVALID.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

class CertVerifyProcNameNormalizationTest : public CertVerifyProcInternalTest {
 protected:
  std::string HistogramName() const {
    std::string prefix("Net.CertVerifier.NameNormalizationPrivateRoots.");
    switch (verify_proc_type()) {
      case CERT_VERIFY_PROC_ANDROID:
        return prefix + "Android";
      case CERT_VERIFY_PROC_IOS:
        return prefix + "IOS";
      case CERT_VERIFY_PROC_BUILTIN:
      case CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS:
        return prefix + "Builtin";
    }
  }

  void ExpectNormalizationHistogram(int verify_error) {
    if (verify_error == OK) {
      histograms_.ExpectUniqueSample(
          HistogramName(), CertVerifyProc::NameNormalizationResult::kNormalized,
          1);
    } else {
      histograms_.ExpectTotalCount(HistogramName(), 0);
    }
  }

  void ExpectByteEqualHistogram() {
    histograms_.ExpectUniqueSample(
        HistogramName(), CertVerifyProc::NameNormalizationResult::kByteEqual,
        1);
  }

 private:
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcNameNormalizationTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

// Tries to verify a chain where the leaf's issuer CN is PrintableString, while
// the intermediate's subject CN is UTF8String, and verifies the proper
// histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, StringType) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  std::string issuer_cn = CertBuilder::MakeRandomHexString(12);
  leaf->SetIssuerTLV(CertBuilder::BuildNameWithCommonNameOfType(
      issuer_cn, CBS_ASN1_PRINTABLESTRING));
  intermediate->SetSubjectTLV(CertBuilder::BuildNameWithCommonNameOfType(
      issuer_cn, CBS_ASN1_UTF8STRING));

  ScopedTestRoot scoped_root(root->GetX509Certificate());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf->GetX509CertificateChain().get(), "www.example.com",
                     flags, &verify_result);

  switch (verify_proc_type()) {
    case CERT_VERIFY_PROC_IOS:
      EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
      break;
    case CERT_VERIFY_PROC_ANDROID:
    case CERT_VERIFY_PROC_BUILTIN:
    case CERT_VERIFY_PROC_BUILTIN_CHROME_ROOTS:
      EXPECT_THAT(error, IsOk());
      break;
  }

  ExpectNormalizationHistogram(error);
}

// Tries to verify a chain where the leaf's issuer CN and intermediate's
// subject CN are both PrintableString but have differing case on the first
// character, and verifies the proper histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, CaseFolding) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  std::string issuer_hex = CertBuilder::MakeRandomHexString(12);
  leaf->SetIssuerTLV(CertBuilder::BuildNameWithCommonNameOfType(
      "Z" + issuer_hex, CBS_ASN1_PRINTABLESTRING));
  intermediate->SetSubjectTLV(CertBuilder::BuildNameWithCommonNameOfType(
      "z" + issuer_hex, CBS_ASN1_PRINTABLESTRING));

  ScopedTestRoot scoped_root(root->GetX509Certificate());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf->GetX509CertificateChain().get(), "www.example.com",
                     flags, &verify_result);

  EXPECT_THAT(error, IsOk());
  ExpectNormalizationHistogram(error);
}

// Confirms that a chain generated by the same pattern as the other
// NameNormalizationTest cases which does not require normalization validates
// ok, and that the ByteEqual histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, ByteEqual) {
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  std::string issuer_hex = CertBuilder::MakeRandomHexString(12);
  leaf->SetIssuerTLV(CertBuilder::BuildNameWithCommonNameOfType(
      issuer_hex, CBS_ASN1_PRINTABLESTRING));
  intermediate->SetSubjectTLV(CertBuilder::BuildNameWithCommonNameOfType(
      issuer_hex, CBS_ASN1_PRINTABLESTRING));

  ScopedTestRoot scoped_root(root->GetX509Certificate());

  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(leaf->GetX509CertificateChain().get(), "www.example.com",
                     flags, &verify_result);

  EXPECT_THAT(error, IsOk());
  ExpectByteEqualHistogram();
}

std::string Md5WithRSAEncryption() {
  const uint8_t kMd5WithRSAEncryption[] = {0x30, 0x0d, 0x06, 0x09, 0x2a,
                                           0x86, 0x48, 0x86, 0xf7, 0x0d,
                                           0x01, 0x01, 0x04, 0x05, 0x00};
  return std::string(std::begin(kMd5WithRSAEncryption),
                     std::end(kMd5WithRSAEncryption));
}

// This is the same as CertVerifyProcInternalTest, but it additionally sets up
// networking capabilities for the cert verifiers, and a test server that can be
// used to serve mock responses for AIA/OCSP/CRL.
//
// An actual HTTP test server is used rather than simply mocking the network
// layer, since the certificate fetching networking layer is not mockable for
// all of the cert verifier implementations.
//
// The approach taken in this test fixture is to generate certificates
// on the fly so they use randomly chosen URLs, subjects, and serial
// numbers, in order to defeat global caching effects from the platform
// verifiers. Moreover, the AIA needs to be chosen dynamically since the
// test server's port number cannot be known statically.
class CertVerifyProcInternalWithNetFetchingTest
    : public CertVerifyProcInternalTest {
 protected:
  CertVerifyProcInternalWithNetFetchingTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT) {}

  void SetUp() override {
    // Create a network thread to be used for network fetches, and wait for
    // initialization to complete on that thread.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    network_thread_ = std::make_unique<base::Thread>("network_thread");
    CHECK(network_thread_->StartWithOptions(std::move(options)));

    base::WaitableEvent initialization_complete_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SetUpOnNetworkThread, &context_, &cert_net_fetcher_,
                       &initialization_complete_event));
    initialization_complete_event.Wait();
    EXPECT_TRUE(cert_net_fetcher_);

    CertVerifyProcInternalTest::SetUp();

    EXPECT_FALSE(test_server_.Started());

    // Register a single request handler with the EmbeddedTestServer, that in
    // turn dispatches to the internally managed registry of request handlers.
    //
    // This allows registering subsequent handlers dynamically during the course
    // of the test, since EmbeddedTestServer requires its handlers be registered
    // prior to Start().
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &CertVerifyProcInternalWithNetFetchingTest::DispatchToRequestHandler,
        base::Unretained(this)));
    EXPECT_TRUE(test_server_.Start());
  }

  void SetUpCertVerifyProc(scoped_refptr<CRLSet> crl_set) override {
    EXPECT_TRUE(cert_net_fetcher_);
    SetUpWithCertNetFetcher(cert_net_fetcher_, std::move(crl_set),
                            /*additional_trust_anchors=*/{},
                            /*additional_untrusted_authorities=*/{});
  }

  void TearDown() override {
    // Do cleanup on network thread.
    network_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ShutdownOnNetworkThread, &context_,
                                  &cert_net_fetcher_));
    network_thread_->Stop();
    network_thread_.reset();

    CertVerifyProcInternalTest::TearDown();
  }

  // Registers a handler with the test server that responds with the given
  // Content-Type, HTTP status code, and response body, for GET requests
  // to |path|.
  // Returns the full URL to |path| for the current test server.
  GURL RegisterSimpleTestServerHandler(std::string path,
                                       HttpStatusCode status_code,
                                       std::string content_type,
                                       std::string content) {
    GURL handler_url(GetTestServerAbsoluteUrl(path));
    base::AutoLock lock(request_handlers_lock_);
    request_handlers_.push_back(base::BindRepeating(
        &SimpleTestServerHandler, std::move(path), status_code,
        std::move(content_type), std::move(content)));
    return handler_url;
  }

  // Returns a random URL path (starting with /) that has the given suffix.
  static std::string MakeRandomPath(std::string_view suffix) {
    return "/" + MakeRandomHexString(12) + std::string(suffix);
  }

  // Returns a URL to |path| for the current test server.
  GURL GetTestServerAbsoluteUrl(const std::string& path) {
    return test_server_.GetURL(path);
  }

  // Creates a certificate chain for www.example.com, where the leaf certificate
  // has an AIA URL pointing to the test server.
  void CreateSimpleChainWithAIA(
      scoped_refptr<X509Certificate>* out_leaf,
      std::string* ca_issuers_path,
      bssl::UniquePtr<CRYPTO_BUFFER>* out_intermediate,
      scoped_refptr<X509Certificate>* out_root) {
    auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

    // Make the leaf certificate have an AIA (CA Issuers) that points to the
    // embedded test server. This uses a random URL for predictable behavior in
    // the presence of global caching.
    *ca_issuers_path = MakeRandomPath(".cer");
    GURL ca_issuers_url = GetTestServerAbsoluteUrl(*ca_issuers_path);
    leaf->SetCaIssuersUrl(ca_issuers_url);

    // The chain being verified is solely the leaf certificate (missing the
    // intermediate and root).
    *out_leaf = leaf->GetX509Certificate();
    *out_root = root->GetX509Certificate();
    *out_intermediate = intermediate->DupCertBuffer();
  }

  // Creates a CRL issued and signed by |crl_issuer|, marking |revoked_serials|
  // as revoked, and registers it to be served by the test server.
  // Returns the full URL to retrieve the CRL from the test server.
  GURL CreateAndServeCrl(CertBuilder* crl_issuer,
                         const std::vector<uint64_t>& revoked_serials,
                         std::optional<bssl::SignatureAlgorithm>
                             signature_algorithm = std::nullopt) {
    std::string crl = BuildCrl(crl_issuer->GetSubject(), crl_issuer->GetKey(),
                               revoked_serials, signature_algorithm);
    std::string crl_path = MakeRandomPath(".crl");
    return RegisterSimpleTestServerHandler(crl_path, HTTP_OK,
                                           "application/pkix-crl", crl);
  }

  GURL CreateAndServeCrlWithAlgorithmTlvAndDigest(
      CertBuilder* crl_issuer,
      const std::vector<uint64_t>& revoked_serials,
      const std::string& signature_algorithm_tlv,
      const EVP_MD* digest) {
    std::string crl = BuildCrlWithAlgorithmTlvAndDigest(
        crl_issuer->GetSubject(), crl_issuer->GetKey(), revoked_serials,
        signature_algorithm_tlv, digest);
    std::string crl_path = MakeRandomPath(".crl");
    return RegisterSimpleTestServerHandler(crl_path, HTTP_OK,
                                           "application/pkix-crl", crl);
  }

 private:
  std::unique_ptr<test_server::HttpResponse> DispatchToRequestHandler(
      const test_server::HttpRequest& request) {
    // Called on the embedded test server's IO thread.
    base::AutoLock lock(request_handlers_lock_);
    for (const auto& handler : request_handlers_) {
      auto response = handler.Run(request);
      if (response)
        return response;
    }

    return nullptr;
  }

  // Serves (|status_code|, |content_type|, |content|) in response to GET
  // requests for |path|.
  static std::unique_ptr<test_server::HttpResponse> SimpleTestServerHandler(
      const std::string& path,
      HttpStatusCode status_code,
      const std::string& content_type,
      const std::string& content,
      const test_server::HttpRequest& request) {
    if (request.relative_url != path)
      return nullptr;

    auto http_response = std::make_unique<test_server::BasicHttpResponse>();

    http_response->set_code(status_code);
    http_response->set_content_type(content_type);
    http_response->set_content(content);
    return http_response;
  }

  static void SetUpOnNetworkThread(
      std::unique_ptr<URLRequestContext>* context,
      scoped_refptr<CertNetFetcherURLRequest>* cert_net_fetcher,
      base::WaitableEvent* initialization_complete_event) {
    URLRequestContextBuilder url_request_context_builder;
    url_request_context_builder.set_user_agent("cert_verify_proc_unittest/0.1");
    url_request_context_builder.set_proxy_config_service(
        std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation()));
    *context = url_request_context_builder.Build();

    *cert_net_fetcher = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
    (*cert_net_fetcher)->SetURLRequestContext(context->get());
    initialization_complete_event->Signal();
  }

  static void ShutdownOnNetworkThread(
      std::unique_ptr<URLRequestContext>* context,
      scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher) {
    (*cert_net_fetcher)->Shutdown();
    cert_net_fetcher->reset();
    context->reset();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::Thread> network_thread_;

  // Owned by this thread, but initialized, used, and shutdown on the network
  // thread.
  std::unique_ptr<URLRequestContext> context_;
  scoped_refptr<CertNetFetcherURLRequest> cert_net_fetcher_;

  EmbeddedTestServer test_server_;

  // The list of registered handlers. Can only be accessed when the lock is
  // held, as this data is shared between the embedded server's IO thread, and
  // the test main thread.
  base::Lock request_handlers_lock_;
  std::vector<test_server::EmbeddedTestServer::HandleRequestCallback>
      request_handlers_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcInternalWithNetFetchingTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA, however the server responds with a 404.
//
// NOTE: This test is separate from IntermediateFromAia200 as a different URL
// needs to be used to avoid having the result depend on globally cached success
// or failure of the fetch.
// Test is flaky on iOS crbug.com/860189
#if BUILDFLAG(IS_IOS)
#define MAYBE_IntermediateFromAia404 DISABLED_IntermediateFromAia404
#else
#define MAYBE_IntermediateFromAia404 IntermediateFromAia404
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia404) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  // Serve a 404 for the AIA url.
  RegisterSimpleTestServerHandler(ca_issuers_path, HTTP_NOT_FOUND, "text/plain",
                                  "Not Found");

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root);

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should fail as the intermediate is missing, and
  // cannot be fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, &verify_result);
  EXPECT_NE(OK, error);

  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}
#undef MAYBE_IntermediateFromAia404

// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA.
// TODO(crbug.com/41399468): Failing on iOS
#if BUILDFLAG(IS_IOS)
#define MAYBE_IntermediateFromAia200Der DISABLED_IntermediateFromAia200Der
#else
#define MAYBE_IntermediateFromAia200Der IntermediateFromAia200Der
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Der) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/pkix-cert",
      std::string(x509_util::CryptoBufferAsStringPiece(intermediate.get())));

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root);

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  // VERIFY_DISABLE_NETWORK_FETCHES flag is not implemented in
  // CertVerifyProcIOS, only test it on other verifiers.
  if (verify_proc_type() != CERT_VERIFY_PROC_IOS) {
    CertVerifyResult verify_result;
    // If VERIFY_DISABLE_NETWORK_FETCHES is specified, AIA should not be
    // attempted and verifying the chain should fail since the intermediate
    // can't be found.
    int error =
        Verify(leaf.get(), kHostname,
               CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES, &verify_result);
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_EQ(0u, verify_result.verified_cert->intermediate_buffers().size());
  }

  {
    CertVerifyResult verify_result;
    // Verifying the chain should succeed as the missing intermediate can be
    // fetched via AIA.
    int error = Verify(leaf.get(), kHostname, /*flags=*/0, &verify_result);
    EXPECT_THAT(error, IsOk());
  }
}

// This test is the same as IntermediateFromAia200Der, except the certificate is
// served as PEM rather than DER.
//
// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA, however is served as a PEM file rather
// than DER.
// TODO(crbug.com/41399468): Failing on iOS
#if BUILDFLAG(IS_IOS)
#define MAYBE_IntermediateFromAia200Pem DISABLED_IntermediateFromAia200Pem
#else
#define MAYBE_IntermediateFromAia200Pem IntermediateFromAia200Pem
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Pem) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  std::string intermediate_pem;
  ASSERT_TRUE(
      X509Certificate::GetPEMEncoded(intermediate.get(), &intermediate_pem));

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/x-x509-ca-cert", intermediate_pem);

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root);

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should succeed as the missing intermediate can be
  // fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    // Android doesn't support PEM - https://crbug.com/725180
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(error, IsOk());
  }

}

// This test is the same as IntermediateFromAia200Pem, but with a different
// formatting on the PEM data.
//
// TODO(crbug.com/41399468): Failing on iOS
#if BUILDFLAG(IS_IOS)
#define MAYBE_IntermediateFromAia200Pem2 DISABLED_IntermediateFromAia200Pem2
#else
#define MAYBE_IntermediateFromAia200Pem2 IntermediateFromAia200Pem2
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Pem2) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  std::string intermediate_pem;
  ASSERT_TRUE(
      X509Certificate::GetPEMEncoded(intermediate.get(), &intermediate_pem));
  intermediate_pem = "Text at start of file\n" + intermediate_pem;

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/x-x509-ca-cert", intermediate_pem);

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root);

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should succeed as the missing intermediate can be
  // fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    // Android doesn't support PEM - https://crbug.com/725180
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(error, IsOk());
  }
}

// Tries verifying a certificate chain that uses a SHA1 intermediate,
// however, chasing the AIA can discover a SHA256 version of the intermediate.
//
// Path building should discover the stronger intermediate and use it.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       Sha1IntermediateButAIAHasSha256) {
  const char kHostname[] = "www.example.com";

  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Make the leaf certificate have an AIA (CA Issuers) that points to the
  // embedded test server. This uses a random URL for predictable behavior in
  // the presence of global caching.
  std::string ca_issuers_path = MakeRandomPath(".cer");
  GURL ca_issuers_url = GetTestServerAbsoluteUrl(ca_issuers_path);
  leaf->SetCaIssuersUrl(ca_issuers_url);
  leaf->SetSubjectAltName(kHostname);

  // Make two versions of the intermediate - one that is SHA256 signed, and one
  // that is SHA1 signed. Note that the subjectKeyIdentifier for `intermediate`
  // is intentionally not changed, so that path building will consider both
  // certificate paths.
  intermediate->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha256);
  intermediate->SetRandomSerialNumber();
  auto intermediate_sha256 = intermediate->DupCertBuffer();

  intermediate->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha1);
  intermediate->SetRandomSerialNumber();
  auto intermediate_sha1 = intermediate->DupCertBuffer();

  // Trust the root certificate.
  auto root_cert = root->GetX509Certificate();
  ScopedTestRoot scoped_root(root_cert);

  // Setup the test server to reply with the SHA256 intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/pkix-cert",
      std::string(
          x509_util::CryptoBufferAsStringPiece(intermediate_sha256.get())));

  // Build a chain to verify that includes the SHA1 intermediate.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_sha1.get()));
  scoped_refptr<X509Certificate> chain_sha1 = X509Certificate::CreateFromBuffer(
      leaf->DupCertBuffer(), std::move(intermediates));
  ASSERT_TRUE(chain_sha1.get());

  const int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(chain_sha1.get(), kHostname, flags, &verify_result);

  if (VerifyProcTypeIsBuiltin()) {
    // Should have built a chain through the SHA256 intermediate. This was only
    // available via AIA, and not the (SHA1) one provided directly to path
    // building.
    ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        verify_result.verified_cert->intermediate_buffers()[0].get(),
        intermediate_sha256.get()));
    ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());

    EXPECT_FALSE(verify_result.has_sha1);
    EXPECT_THAT(error, IsOk());
  } else {
    EXPECT_NE(OK, error);
    if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID &&
        error == ERR_CERT_AUTHORITY_INVALID) {
      // Newer Android versions reject the chain due to the SHA1 intermediate,
      // but do not build the correct chain by AIA. Since only the partial
      // chain is returned, CertVerifyProc does not mark it as SHA1 as it does
      // not examine the last cert in the chain. Therefore, if
      // ERR_CERT_AUTHORITY_INVALID is returned, don't check the rest of the
      // statuses. See https://crbug.com/1191795.
      return;
    }
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
    EXPECT_TRUE(verify_result.has_sha1);
  }
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest, RevocationHardFailNoCrls) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  // Create certs which have no AIA or CRL distribution points.
  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  EXPECT_THAT(error, IsError(ERR_CERT_NO_REVOCATION_MECHANISM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailNoCrlsDisableNetworkFetches) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  // Create certs which have no AIA or CRL distribution points.
  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with flags for both hard-fail revocation checking for local anchors
  // and disabling network fetches.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS |
                    CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should succeed, VERIFY_DISABLE_NETWORK_FETCHES takes priority.
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where both leaf and intermediate are covered by valid
// CRLs which have empty (non-present) revokedCertificates list. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailCrlGoodNoRevokedCertificates) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should pass, leaf and intermediate were covered by CRLs and were not
  // revoked.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where both leaf and intermediate are covered by valid
// CRLs which have revokedCertificates lists that revoke other irrelevant
// serial numbers. Verification should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailCrlGoodIrrelevantSerialsRevoked) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL revokes leaf's serial number. This is irrelevant.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {leaf->GetSerialNumber()}));

  // Intermediate-issued CRL revokes intermediates's serial number. This is
  // irrelevant.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {intermediate->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should pass, leaf and intermediate were covered by CRLs and were not
  // revoked.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailLeafRevokedByCrl) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail, leaf is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailIntermediateRevokedByCrl) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Intermediate is revoked by root issued CRL.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {intermediate->GetSerialNumber()}));

  // Intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail, intermediate is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where the intermediate certificate has a good CRL, but
// the leaf's distribution point returns an http error. Verification should
// fail.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailLeafCrlDpHttpError) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve a 404 for the intermediate-issued CRL distribution point url.
  leaf->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail since no revocation information was available for the leaf.
  EXPECT_THAT(error, IsError(ERR_CERT_UNABLE_TO_CHECK_REVOCATION));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where the leaf certificate has a good CRL, but
// the intermediate's distribution point returns an http error. Verification
// should fail.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailIntermediateCrlDpHttpError) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a 404 for the root-issued CRL distribution point url.
  intermediate->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail since no revocation information was available for the
  // intermediate.
  EXPECT_THAT(error, IsError(ERR_CERT_UNABLE_TO_CHECK_REVOCATION));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest, RevocationSoftFailNoCrls) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  // Create certs which have no AIA or CRL distribution points.
  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_NO_REVOCATION_MECHANISM);
  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where both leaf and intermediate are covered by valid
// CRLs which have empty (non-present) revokedCertificates list. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailCrlGoodNoRevokedCertificates) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where both leaf and intermediate are covered by valid
// CRLs which have revokedCertificates lists that revoke other irrelevant
// serial numbers. Verification should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailCrlGoodIrrelevantSerialsRevoked) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL revokes leaf's serial number. This is irrelevant.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {leaf->GetSerialNumber()}));

  // Intermediate-issued CRL revokes intermediates's serial number. This is
  // irrelevant.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {intermediate->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedByCrl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail, leaf is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedByCrlDisableNetworkFetches) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with flags for both soft-fail revocation checking and disabling
  // network fetches.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED |
                    CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should succeed, VERIFY_DISABLE_NETWORK_FETCHES takes priority.
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailIntermediateRevokedByCrl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Intermediate is revoked by root issued CRL.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {intermediate->GetSerialNumber()}));

  // Intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail, intermediate is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedBySha1Crl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL which is signed with
  // ecdsaWithSha256.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()},
                        bssl::SignatureAlgorithm::kEcdsaSha1));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should fail, leaf is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedByMd5Crl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));
  // This test wants to check handling of MD5 CRLs, but ecdsa-with-md5
  // signatureAlgorithm does not exist. Use an RSA private key for intermediate
  // so that the CRL will be signed with the md5WithRSAEncryption algorithm.
  ASSERT_TRUE(intermediate->UseKeyFromFile(
      GetTestCertsDirectory().AppendASCII("rsa-2048-1.key")));
  leaf->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kRsaPkcs1Sha256);

  // Leaf is revoked by intermediate issued CRL which is signed with
  // md5WithRSAEncryption.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrlWithAlgorithmTlvAndDigest(
      intermediate.get(), {leaf->GetSerialNumber()}, Md5WithRSAEncryption(),
      EVP_md5()));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Verification should succeed: MD5 signature algorithm is not supported
  // and soft-fail checking will ignore the inability to get revocation
  // status.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where the intermediate certificate has a good CRL, but
// the leaf's distribution point returns an http error. Verification should
// succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafCrlDpHttpError) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve a 404 for the intermediate-issued CRL distribution point url.
  leaf->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should succeed due to soft-fail revocation checking.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where the leaf certificate has a good CRL, but
// the intermediate's distribution point returns an http error. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailIntermediateCrlDpHttpError) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();

  // Serve a 404 for the root-issued CRL distribution point url.
  intermediate->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error = Verify(chain.get(), kHostname, flags, &verify_result);

  // Should succeed due to soft-fail revocation checking.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// Tests that an EV cert verification with successful online OCSP revocation
// checks is marked as CERT_STATUS_IS_EV.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       EVOnlineOCSPRevocationCheckingGood) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }

  const char kEVTestCertPolicy[] = "1.2.3.4";
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.policy_oids = {kEVTestCertPolicy};
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::GOOD,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  EmbeddedTestServer ocsp_test_server(EmbeddedTestServer::TYPE_HTTPS);
  ocsp_test_server.SetSSLConfig(cert_config);
  EXPECT_TRUE(ocsp_test_server.Start());

  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root.get());

  scoped_refptr<X509Certificate> chain = ocsp_test_server.GetCertificate();
  ASSERT_TRUE(chain.get());

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->cert_buffer()),
      kEVTestCertPolicy);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(chain.get(), ocsp_test_server.host_port_pair().host(),
                     flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// Tests that an EV cert verification with that could not retrieve online OCSP
// revocation information is verified but still marked as CERT_STATUS_IS_EV.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       EVOnlineOCSPRevocationCheckingSoftFail) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }

  const char kEVTestCertPolicy[] = "1.2.3.4";
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.policy_oids = {kEVTestCertPolicy};
  // Retrieving OCSP status returns an error.
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      EmbeddedTestServer::OCSPConfig::ResponseType::kInternalError);

  EmbeddedTestServer ocsp_test_server(EmbeddedTestServer::TYPE_HTTPS);
  ocsp_test_server.SetSSLConfig(cert_config);
  EXPECT_TRUE(ocsp_test_server.Start());

  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root.get());

  scoped_refptr<X509Certificate> chain = ocsp_test_server.GetCertificate();
  ASSERT_TRUE(chain.get());

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->cert_buffer()),
      kEVTestCertPolicy);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(chain.get(), ocsp_test_server.host_port_pair().host(),
                     flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// Tests that an EV cert verification with online OCSP returning affirmatively
// revoked is marked as CERT_STATUS_IS_EV.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       EVOnlineOCSPRevocationCheckingRevoked) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }

  const char kEVTestCertPolicy[] = "1.2.3.4";
  EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.policy_oids = {kEVTestCertPolicy};
  cert_config.ocsp_config = EmbeddedTestServer::OCSPConfig(
      {{bssl::OCSPRevocationStatus::REVOKED,
        EmbeddedTestServer::OCSPConfig::SingleResponse::Date::kValid}});

  EmbeddedTestServer ocsp_test_server(EmbeddedTestServer::TYPE_HTTPS);
  ocsp_test_server.SetSSLConfig(cert_config);
  EXPECT_TRUE(ocsp_test_server.Start());

  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root.get());

  scoped_refptr<X509Certificate> chain = ocsp_test_server.GetCertificate();
  ASSERT_TRUE(chain.get());

  // Consider the root of the test chain a valid EV root for the test policy.
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(),
      X509Certificate::CalculateFingerprint256(root->cert_buffer()),
      kEVTestCertPolicy);

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(chain.get(), ocsp_test_server.host_port_pair().host(),
                     flags, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// A set of tests that check how various constraints are enforced when they
// appear at different points in the chain, such as on the trust anchor versus
// on intermediates.
class CertVerifyProcConstraintsTest : public CertVerifyProcInternalTest {
 protected:
  void SetUp() override {
    CertVerifyProcInternalTest::SetUp();

    chain_ = CertBuilder::CreateSimpleChain(/*chain_length=*/4);
  }

  int VerifyWithTrust(bssl::CertificateTrust trust) {
    ScopedTestRoot test_root(chain_.back()->GetX509Certificate(), trust);
    CertVerifyResult verify_result;
    int flags = 0;
    return CertVerifyProcInternalTest::Verify(
        chain_.front()->GetX509CertificateChain().get(), "www.example.com",
        flags, &verify_result);
  }

  int Verify() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor());
  }

  int VerifyWithExpiryAndConstraints() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor()
                               .WithEnforceAnchorExpiry()
                               .WithEnforceAnchorConstraints());
  }

  int VerifyWithExpiryAndFullConstraints() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor()
                               .WithEnforceAnchorExpiry()
                               .WithEnforceAnchorConstraints()
                               .WithRequireAnchorBasicConstraints());
  }

  int ExpectedIntermediateConstraintError() {
    if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID)
      return ERR_CERT_AUTHORITY_INVALID;
    return ERR_CERT_INVALID;
  }

  std::vector<std::unique_ptr<CertBuilder>> chain_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcConstraintsTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

TEST_P(CertVerifyProcConstraintsTest, BaseCase) {
  // Without changing anything on the test chain, it should validate
  // successfully. If this is not true then the rest of the tests in this class
  // are unlikely to be useful.
  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(), IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()),
                IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()),
                IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotCaRoot) {
  chain_[3]->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotCaIntermediate) {
  chain_[2]->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/-1);

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsIsCaLeaf) {
  for (bool has_key_usage_cert_sign : {false, true}) {
    chain_[0]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);

    if (has_key_usage_cert_sign) {
      chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                               bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    } else {
      chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    }
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsPathlen0Root) {
  chain_[3]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/0);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (VerifyProcTypeIsIOSAtMostOS14() ||
             verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsPathlen1Root) {
  chain_[3]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/1);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (VerifyProcTypeIsIOSAtMostOS14() ||
             verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsPathlen2Root) {
  chain_[3]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/2);

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest,
       BasicConstraintsPathlen0IntermediateParent) {
  chain_[2]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/0);

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest,
       BasicConstraintsPathlen1IntermediateParent) {
  chain_[2]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/1);

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest,
       BasicConstraintsPathlen0IntermediateChild) {
  chain_[1]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/0);

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotPresentRoot) {
  chain_[3]->EraseExtension(bssl::der::Input(bssl::kBasicConstraintsOid));

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotPresentRootX509V1) {
  chain_[3]->SetCertificateVersion(bssl::CertificateVersion::V1);
  chain_[3]->ClearExtensions();

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotPresentIntermediate) {
  chain_[2]->EraseExtension(bssl::der::Input(bssl::kBasicConstraintsOid));

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, BasicConstraintsNotPresentLeaf) {
  chain_[0]->EraseExtension(bssl::der::Input(bssl::kBasicConstraintsOid));

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, NameConstraintsNotMatchingRoot) {
  chain_[3]->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.org"},
                                        /*excluded_dns_names=*/{});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, NameConstraintsNotMatchingIntermediate) {
  chain_[2]->SetNameConstraintsDnsNames(
      /*permitted_dns_names=*/{"example.org"},
      /*excluded_dns_names=*/{});

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, NameConstraintsMatchingRoot) {
  chain_[3]->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.com"},
                                        /*excluded_dns_names=*/{});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, NameConstraintsMatchingIntermediate) {
  chain_[2]->SetNameConstraintsDnsNames(
      /*permitted_dns_names=*/{"example.com"},
      /*excluded_dns_names=*/{});

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, NameConstraintsOnLeaf) {
  chain_[0]->SetNameConstraintsDnsNames(
      /*permitted_dns_names=*/{"example.com"},
      /*excluded_dns_names=*/{});

  // TODO(mattm): this should be an error
  // RFC 5280 4.2.1.10 says: "The name constraints extension, which MUST be
  // used only in a CA certificate, ..."
  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, ValidityExpiredRoot) {
  chain_[3]->SetValidity(base::Time::Now() - base::Days(14),
                         base::Time::Now() - base::Days(7));

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                IsError(ERR_CERT_DATE_INVALID));
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_DATE_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, ValidityNotYetValidRoot) {
  chain_[3]->SetValidity(base::Time::Now() + base::Days(7),
                         base::Time::Now() + base::Days(14));

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                IsError(ERR_CERT_DATE_INVALID));
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_DATE_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, ValidityExpiredIntermediate) {
  chain_[2]->SetValidity(base::Time::Now() - base::Days(14),
                         base::Time::Now() - base::Days(7));

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, ValidityNotYetValidIntermediate) {
  chain_[2]->SetValidity(base::Time::Now() + base::Days(7),
                         base::Time::Now() + base::Days(14));

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints0Root) {
  for (bool leaf_has_policy : {false, true}) {
    SCOPED_TRACE(leaf_has_policy);

    static const char kPolicy1[] = "1.2.3.4";
    static const char kPolicy2[] = "1.2.3.4.5";
    static const char kPolicy3[] = "1.2.3.5";
    chain_[3]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);
    chain_[3]->SetCertificatePolicies({kPolicy1, kPolicy2});
    chain_[2]->SetCertificatePolicies({kPolicy3, kPolicy1});
    chain_[1]->SetCertificatePolicies({kPolicy1});

    if (leaf_has_policy) {
      chain_[0]->SetCertificatePolicies({kPolicy1});
      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    } else {
      chain_[0]->SetCertificatePolicies({});
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
      } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
                 verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
        EXPECT_THAT(Verify(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints4Root) {
  // Explicit policy is required after 4 certs. Since the chain is 4 certs
  // long, an explicit policy is never required.
  chain_[3]->SetPolicyConstraints(
      /*require_explicit_policy=*/4,
      /*inhibit_policy_mapping=*/std::nullopt);

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints3Root) {
  // Explicit policy is required after 3 certs. Since the chain is 4 certs
  // long, an explicit policy is required and the chain should fail if anchor
  // constraints are enforced.
  chain_[3]->SetPolicyConstraints(
      /*require_explicit_policy=*/3,
      /*inhibit_policy_mapping=*/std::nullopt);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else {
    // Windows seems to have an off-by-one error in how it enforces
    // requireExplicitPolicy.
    // (The mac/android verifiers are Ok here since they don't enforce
    // policyConstraints on anchors.)
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints2Root) {
  // Explicit policy is required after 2 certs. Since the chain is 4 certs
  // long, an explicit policy is required and the chain should fail if anchor
  // constraints are enforced.
  chain_[3]->SetPolicyConstraints(
      /*require_explicit_policy=*/2,
      /*inhibit_policy_mapping=*/std::nullopt);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
             verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

// This is also a regression test for https://crbug.com/31497: If an
// intermediate has requireExplicitPolicy in its policyConstraints extension,
// verification should still succeed as long as some policy is valid for the
// chain, since Chrome does not specify any required policy as an input to
// certificate verification (allows anyPolicy).
TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints0Intermediate) {
  for (bool leaf_has_policy : {false, true}) {
    SCOPED_TRACE(leaf_has_policy);

    static const char kPolicy1[] = "1.2.3.4";
    static const char kPolicy2[] = "1.2.3.4.5";
    static const char kPolicy3[] = "1.2.3.5";
    chain_[2]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);
    chain_[2]->SetCertificatePolicies({kPolicy1, kPolicy2});
    chain_[1]->SetCertificatePolicies({kPolicy3, kPolicy1});

    if (leaf_has_policy) {
      chain_[0]->SetCertificatePolicies({kPolicy1});
      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    } else {
      chain_[0]->SetCertificatePolicies({});
      EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints3Intermediate) {
  // Explicit policy is required after 3 certs. Since the chain up to
  // |chain_[2]| is 3 certs long, an explicit policy is never required.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/3,
      /*inhibit_policy_mapping=*/std::nullopt);

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints2Intermediate) {
  // Explicit policy is required after 2 certs. Since the chain up to
  // |chain_[2]| is 3 certs long, an explicit policy will be required and this
  // should fail to verify.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/2,
      /*inhibit_policy_mapping=*/std::nullopt);

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
    }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints1Intermediate) {
  // Explicit policy is required after 1 cert. Since the chain up to
  // |chain_[2]| is 3 certs long, an explicit policy will be required and this
  // should fail to verify.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/1,
      /*inhibit_policy_mapping=*/std::nullopt);

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyConstraints0Leaf) {
  // Setting requireExplicitPolicy to 0 on the target certificate should make
  // an explicit policy required for the chain. (Ref: RFC 5280 section 6.1.5.b
  // and the final paragraph of 6.1.5)
  chain_[0]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, InhibitPolicyMapping0Root) {
  static const char kPolicy1[] = "1.2.3.4";
  static const char kPolicy2[] = "1.2.3.5";

  // Root inhibits policy mapping immediately.
  chain_[3]->SetPolicyConstraints(
      /*require_explicit_policy=*/std::nullopt,
      /*inhibit_policy_mapping=*/0);

  // Policy constraints are specified on an intermediate so that an explicit
  // policy will be required regardless if root constraints are applied.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  // Intermediate uses policy mappings. This should not be allowed if the root
  // constraints were enforced.
  chain_[2]->SetCertificatePolicies({kPolicy1});
  chain_[2]->SetPolicyMappings({{kPolicy1, kPolicy2}});

  // Children require the policy mapping to have a valid policy.
  chain_[1]->SetCertificatePolicies({kPolicy2});
  chain_[0]->SetCertificatePolicies({kPolicy2});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
             verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    // Windows enforces inhibitPolicyMapping on the root.
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, InhibitPolicyMapping1Root) {
  static const char kPolicy1[] = "1.2.3.4";
  static const char kPolicy2[] = "1.2.3.5";

  // Root inhibits policy mapping after 1 cert.
  chain_[3]->SetPolicyConstraints(
      /*require_explicit_policy=*/std::nullopt,
      /*inhibit_policy_mapping=*/1);

  // Policy constraints are specified on an intermediate so that an explicit
  // policy will be required regardless if root constraints are applied.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  // Intermediate uses policy mappings. This should be allowed even if the root
  // constraints were enforced, since policy mapping was allowed for 1 cert
  // following the root.
  chain_[2]->SetCertificatePolicies({kPolicy1});
  chain_[2]->SetPolicyMappings({{kPolicy1, kPolicy2}});

  // Children require the policy mapping to have a valid policy.
  chain_[1]->SetCertificatePolicies({kPolicy2});
  chain_[0]->SetCertificatePolicies({kPolicy2});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, InhibitAnyPolicy0Root) {
  static const char kAnyPolicy[] = "2.5.29.32.0";
  static const char kPolicy1[] = "1.2.3.4";

  // Since inhibitAnyPolicy is 0, anyPolicy should not be allow for any certs
  // after the root.
  chain_[3]->SetInhibitAnyPolicy(0);
  chain_[3]->SetCertificatePolicies({kAnyPolicy});

  // Policy constraints are specified on an intermediate so that an explicit
  // policy will be required regardless if root constraints are applied.
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  // This intermediate only asserts anyPolicy, so this chain should
  // be invalid if policyConstraints from the root cert are enforced.
  chain_[2]->SetCertificatePolicies({kAnyPolicy});

  chain_[1]->SetCertificatePolicies({kPolicy1});
  chain_[0]->SetCertificatePolicies({kPolicy1});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
             verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, InhibitAnyPolicy1Root) {
  for (bool chain_1_has_any_policy : {false, true}) {
    SCOPED_TRACE(chain_1_has_any_policy);

    static const char kAnyPolicy[] = "2.5.29.32.0";
    static const char kPolicy1[] = "1.2.3.4";

    // Since inhibitAnyPolicy is 1, anyPolicy should be allowed for the root's
    // immediate child, but not after that.
    chain_[3]->SetInhibitAnyPolicy(1);
    chain_[3]->SetCertificatePolicies({kAnyPolicy});

    // Policy constraints are specified on an intermediate so that an explicit
    // policy will be required regardless if root constraints are applied.
    chain_[2]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);

    // AnyPolicy should be allowed in this cert.
    chain_[2]->SetCertificatePolicies({kAnyPolicy});

    chain_[0]->SetCertificatePolicies({kPolicy1});

    if (chain_1_has_any_policy) {
      // AnyPolicy should not be allowed in this cert if the inhibitAnyPolicy
      // constraint from the root is honored.
      chain_[1]->SetCertificatePolicies({kAnyPolicy});

      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
      } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
                 verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
        EXPECT_THAT(Verify(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
      }
    } else {
      chain_[1]->SetCertificatePolicies({kPolicy1});

      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, InhibitAnyPolicy0Intermediate) {
  static const char kAnyPolicy[] = "2.5.29.32.0";
  static const char kPolicy1[] = "1.2.3.4";

  chain_[2]->SetInhibitAnyPolicy(0);
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  chain_[2]->SetCertificatePolicies({kAnyPolicy});
  // This shouldn't be allowed as the parent cert set inhibitAnyPolicy=0.
  chain_[1]->SetCertificatePolicies({kAnyPolicy});
  chain_[0]->SetCertificatePolicies({kPolicy1});

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, InhibitAnyPolicy1Intermediate) {
  static const char kAnyPolicy[] = "2.5.29.32.0";
  static const char kPolicy1[] = "1.2.3.4";

  chain_[2]->SetInhibitAnyPolicy(1);
  chain_[2]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);

  chain_[2]->SetCertificatePolicies({kAnyPolicy});
  // This is okay as the parent cert set inhibitAnyPolicy=1.
  chain_[1]->SetCertificatePolicies({kAnyPolicy});
  chain_[0]->SetCertificatePolicies({kPolicy1});

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, PoliciesRoot) {
  static const char kPolicy1[] = "1.2.3.4";
  static const char kPolicy2[] = "1.2.3.5";

  for (bool root_has_matching_policy : {false, true}) {
    SCOPED_TRACE(root_has_matching_policy);

    if (root_has_matching_policy) {
      // This chain should be valid whether or not policies from the root are
      // processed.
      chain_[3]->SetCertificatePolicies({kPolicy1});
    } else {
      // If the policies from the root are processed, this chain will not be
      // valid for any policy.
      chain_[3]->SetCertificatePolicies({kPolicy2});
    }

    // Policy constraints are specified on an intermediate so that an explicit
    // policy will be required regardless if root constraints are applied.
    chain_[2]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);

    chain_[2]->SetCertificatePolicies({kPolicy1});
    chain_[1]->SetCertificatePolicies({kPolicy1});
    chain_[0]->SetCertificatePolicies({kPolicy1});

    if (root_has_matching_policy) {
      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    } else {
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
      } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
                 verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
        EXPECT_THAT(Verify(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, PolicyMappingsRoot) {
  static const char kPolicy1[] = "1.2.3.4";
  static const char kPolicy2[] = "1.2.3.5";
  static const char kPolicy3[] = "1.2.3.6";

  for (bool root_has_matching_policy_mapping : {false, true}) {
    SCOPED_TRACE(root_has_matching_policy_mapping);

    if (root_has_matching_policy_mapping) {
      // This chain should be valid if the policies and policy mapping on the
      // root are processed, or if neither is processed. It will not be valid
      // if the policies were processed and the policyMappings were not.
      chain_[3]->SetCertificatePolicies({kPolicy1});
      chain_[3]->SetPolicyMappings({{kPolicy1, kPolicy2}});
    } else {
      // This chain should not be valid if the policies and policyMappings on
      // the root were processed. It will be valid if the policies were
      // processed and policyMappings were not.
      chain_[3]->SetCertificatePolicies({kPolicy2});
      chain_[3]->SetPolicyMappings({{kPolicy2, kPolicy3}});
    }

    // Policy constraints are specified on an intermediate so that an explicit
    // policy will be required regardless if root constraints are applied.
    chain_[2]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);

    chain_[2]->SetCertificatePolicies({kPolicy2});
    chain_[1]->SetCertificatePolicies({kPolicy2});
    chain_[0]->SetCertificatePolicies({kPolicy2});

    if (root_has_matching_policy_mapping) {
      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    } else {
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
      } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
                 verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
        EXPECT_THAT(Verify(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNoCertSignRoot) {
  chain_[3]->SetKeyUsages({bssl::KEY_USAGE_BIT_CRL_SIGN});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNotPresentRoot) {
  chain_[3]->EraseExtension(bssl::der::Input(bssl::kKeyUsageOid));

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNoCertSignIntermediate) {
  chain_[2]->SetKeyUsages({bssl::KEY_USAGE_BIT_CRL_SIGN});

  EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNotPresentIntermediate) {
  chain_[2]->EraseExtension(bssl::der::Input(bssl::kKeyUsageOid));

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNoDigitalSignatureLeaf) {
  // This test is mostly uninteresting since keyUsage on the end-entity is only
  // checked at the TLS layer, not during cert verification.
  chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_CRL_SIGN});

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageNotPresentLeaf) {
  // This test is mostly uninteresting since keyUsage on the end-entity is only
  // checked at the TLS layer, not during cert verification.
  chain_[0]->EraseExtension(bssl::der::Input(bssl::kKeyUsageOid));

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, KeyUsageCertSignLeaf) {
  // Test a leaf that has keyUsage asserting keyCertSign and basicConstraints
  // asserting CA=false. This should be an error according to 5280 section
  // 4.2.1.3 and 4.2.1.9, however most implementations seem to allow it.
  // Perhaps because 5280 section 6 does not explicitly say to enforce this on
  // the target cert.
  chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                           bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, ExtendedKeyUsageNoServerAuthRoot) {
  chain_[3]->SetExtendedKeyUsages({bssl::der::Input(bssl::kCodeSigning)});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsOk());
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsError(ERR_CERT_INVALID));
    EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                IsError(ERR_CERT_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID ||
             verify_proc_type() == CERT_VERIFY_PROC_IOS) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, ExtendedKeyUsageServerAuthRoot) {
  chain_[3]->SetExtendedKeyUsages({bssl::der::Input(bssl::kServerAuth)});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest,
       ExtendedKeyUsageNoServerAuthIntermediate) {
  chain_[2]->SetExtendedKeyUsages({bssl::der::Input(bssl::kCodeSigning)});

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID ||
      VerifyProcTypeIsIOSAtMostOS15()) {
    EXPECT_THAT(Verify(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, ExtendedKeyUsageServerAuthIntermediate) {
  chain_[2]->SetExtendedKeyUsages({bssl::der::Input(bssl::kServerAuth)});

  EXPECT_THAT(Verify(), IsOk());
}

TEST_P(CertVerifyProcConstraintsTest, ExtendedKeyUsageNoServerAuthLeaf) {
  chain_[0]->SetExtendedKeyUsages({bssl::der::Input(bssl::kCodeSigning)});

  EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
}

TEST_P(CertVerifyProcConstraintsTest, UnknownSignatureAlgorithmRoot) {
  chain_[3]->SetSignatureAlgorithmTLV(TestOid0SignatureAlgorithmTLV());

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTest, UnknownSignatureAlgorithmIntermediate) {
  chain_[2]->SetSignatureAlgorithmTLV(TestOid0SignatureAlgorithmTLV());

  if (verify_proc_type() == CERT_VERIFY_PROC_IOS) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
  }
}

TEST_P(CertVerifyProcConstraintsTest, UnknownSignatureAlgorithmLeaf) {
  chain_[0]->SetSignatureAlgorithmTLV(TestOid0SignatureAlgorithmTLV());

  if (verify_proc_type() == CERT_VERIFY_PROC_IOS) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTest, UnknownExtensionRoot) {
  for (bool critical : {true, false}) {
    SCOPED_TRACE(critical);
    chain_[3]->SetExtension(TestOid0(), "hello world", critical);

    if (critical) {
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyWithExpiryAndConstraints(),
                    IsError(ERR_CERT_INVALID));
        EXPECT_THAT(VerifyWithExpiryAndFullConstraints(),
                    IsError(ERR_CERT_INVALID));
      } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS ||
                 verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
        EXPECT_THAT(Verify(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
      }
    } else {
      EXPECT_THAT(Verify(), IsOk());
      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(VerifyWithExpiryAndConstraints(), IsOk());
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, UnknownExtensionIntermediate) {
  for (bool critical : {true, false}) {
    SCOPED_TRACE(critical);
    chain_[2]->SetExtension(TestOid0(), "hello world", critical);

    if (critical) {
      EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
    } else {
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

TEST_P(CertVerifyProcConstraintsTest, UnknownExtensionLeaf) {
  for (bool critical : {true, false}) {
    SCOPED_TRACE(critical);
    chain_[0]->SetExtension(TestOid0(), "hello world", critical);

    if (critical) {
      EXPECT_THAT(Verify(), IsError(ExpectedIntermediateConstraintError()));
    } else {
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

// A set of tests that check how various constraints are enforced when they
// are applied to a directly trusted non-self-signed leaf certificate.
class CertVerifyProcConstraintsTrustedLeafTest
    : public CertVerifyProcInternalTest {
 protected:
  void SetUp() override {
    CertVerifyProcInternalTest::SetUp();

    chain_ = CertBuilder::CreateSimpleChain(/*chain_length=*/2);
  }

  int VerifyWithTrust(bssl::CertificateTrust trust) {
    ScopedTestRoot test_root(chain_[0]->GetX509Certificate(), trust);
    CertVerifyResult verify_result;
    int flags = 0;
    return CertVerifyProcInternalTest::Verify(
        chain_.front()->GetX509Certificate().get(), "www.example.com", flags,
        &verify_result);
  }

  int Verify() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor());
  }

  int VerifyAsTrustedLeaf() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf());
  }

  std::vector<std::unique_ptr<CertBuilder>> chain_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcConstraintsTrustedLeafTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, BaseCase) {
  // Without changing anything on the test chain, it should validate
  // successfully. If this is not true then the rest of the tests in this class
  // are unlikely to be useful.
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()),
                IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()
                                    .WithRequireLeafSelfSigned()),
                IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                    .WithRequireLeafSelfSigned()),
                IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, RootAlsoTrusted) {
  // Test verifying a chain where both the leaf and the root are marked as
  // trusted.
  // (Repeating the ScopedTestRoot before each call is due to the limitation
  // with destroying any ScopedTestRoot removing all test roots.)
  {
    ScopedTestRoot test_root(chain_[1]->GetX509Certificate());
    EXPECT_THAT(Verify(), IsOk());
  }

  if (VerifyProcTypeIsBuiltin()) {
    {
      ScopedTestRoot test_root1(chain_[1]->GetX509Certificate());
      // An explicit trust entry for the leaf with a value of Unspecified
      // should be no different than the leaf not being in the trust store at
      // all.
      EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForUnspecified()),
                  IsOk());
    }
    {
      ScopedTestRoot test_root1(chain_[1]->GetX509Certificate());
      // If the leaf is explicitly distrusted, verification should fail even if
      // the root is trusted.
      EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForDistrusted()),
                  IsError(ERR_CERT_AUTHORITY_INVALID));
    }
    {
      ScopedTestRoot test_root(chain_[1]->GetX509Certificate());
      EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
    }
    {
      ScopedTestRoot test_root(chain_[1]->GetX509Certificate());
      EXPECT_THAT(
          VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()),
          IsOk());
    }
    {
      ScopedTestRoot test_root(chain_[1]->GetX509Certificate());
      EXPECT_THAT(
          VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()),
          IsOk());
    }
    {
      ScopedTestRoot test_root(chain_[1]->GetX509Certificate());
      EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                      .WithRequireLeafSelfSigned()),
                  IsOk());
    }
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, BasicConstraintsIsCa) {
  for (bool has_key_usage_cert_sign : {false, true}) {
    chain_[0]->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);

    if (has_key_usage_cert_sign) {
      chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                               bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    } else {
      chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    }

    if (VerifyProcTypeIsBuiltin()) {
      EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
      EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
    } else {
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, BasicConstraintsPathlen) {
  chain_[0]->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/0);

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, BasicConstraintsMissing) {
  chain_[0]->EraseExtension(bssl::der::Input(bssl::kBasicConstraintsOid));

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, NameConstraintsNotMatching) {
  chain_[0]->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.org"},
                                        /*excluded_dns_names=*/{});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, ValidityExpired) {
  chain_[0]->SetValidity(base::Time::Now() - base::Days(14),
                         base::Time::Now() - base::Days(7));

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsError(ERR_CERT_DATE_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, PolicyConstraints) {
  static const char kPolicy1[] = "1.2.3.4";

  for (bool leaf_has_policy : {false, true}) {
    SCOPED_TRACE(leaf_has_policy);

    chain_[0]->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);
    if (leaf_has_policy) {
      chain_[0]->SetCertificatePolicies({kPolicy1});
    } else {
      chain_[0]->SetCertificatePolicies({});
    }

    if (VerifyProcTypeIsBuiltin()) {
      EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
      EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
    } else {
      // Succeeds since the ios/android verifiers appear to not enforce
      // this constraint in the "directly trusted leaf" case.
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, InhibitAnyPolicy) {
  static const char kAnyPolicy[] = "2.5.29.32.0";
  chain_[0]->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);
  chain_[0]->SetInhibitAnyPolicy(0);
  chain_[0]->SetCertificatePolicies({kAnyPolicy});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, KeyUsageNoDigitalSignature) {
  // This test is mostly uninteresting since keyUsage on the end-entity is only
  // checked at the TLS layer, not during cert verification.
  chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_CRL_SIGN});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, KeyUsageCertSignLeaf) {
  // Test a leaf that has keyUsage asserting keyCertSign with basicConstraints
  // CA=false, which is an error according to 5280 (4.2.1.3 and 4.2.1.9).
  chain_[0]->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                           bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, ExtendedKeyUsageNoServerAuth) {
  chain_[0]->SetExtendedKeyUsages({bssl::der::Input(bssl::kCodeSigning)});

  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsError(ERR_CERT_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, UnknownSignatureAlgorithm) {
  chain_[0]->SetSignatureAlgorithmTLV(TestOid0SignatureAlgorithmTLV());

  if (VerifyProcTypeIsBuiltin()) {
    // Since no chain is found, signature is not checked, fails with generic
    // error for untrusted chain.
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
    // Valid since signature on directly trusted leaf is not checked.
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, WeakSignatureAlgorithm) {
  chain_[0]->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha1);

  if (VerifyProcTypeIsBuiltin()) {
    // Since no chain is found, signature is not checked, fails with generic
    // error for untrusted chain.
    EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));

    // Valid since signature on directly trusted leaf is not checked.
    EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());

    // Cert is not self-signed so directly trusted leaf with
    // require_leaf_selfsigned should fail.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()
                                    .WithRequireLeafSelfSigned()),
                IsError(ERR_CERT_AUTHORITY_INVALID));
  } else if (verify_proc_type() == CERT_VERIFY_PROC_IOS) {
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedLeafTest, UnknownExtension) {
  for (bool critical : {true, false}) {
    SCOPED_TRACE(critical);
    chain_[0]->SetExtension(TestOid0(), "hello world", critical);

    if (VerifyProcTypeIsBuiltin()) {
      EXPECT_THAT(Verify(), IsError(ERR_CERT_AUTHORITY_INVALID));
      if (critical) {
        EXPECT_THAT(VerifyAsTrustedLeaf(), IsError(ERR_CERT_INVALID));
      } else {
        EXPECT_THAT(VerifyAsTrustedLeaf(), IsOk());
      }
    } else {
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

// A set of tests that check how various constraints are enforced when they
// are applied to a directly trusted self-signed leaf certificate.
class CertVerifyProcConstraintsTrustedSelfSignedTest
    : public CertVerifyProcInternalTest {
 protected:
  void SetUp() override {
    CertVerifyProcInternalTest::SetUp();

    cert_ = std::move(CertBuilder::CreateSimpleChain(/*chain_length=*/1)[0]);
  }

  int VerifyWithTrust(bssl::CertificateTrust trust) {
    ScopedTestRoot test_root(cert_->GetX509Certificate(), trust);
    CertVerifyResult verify_result;
    int flags = 0;
    return CertVerifyProcInternalTest::Verify(cert_->GetX509Certificate().get(),
                                              "www.example.com", flags,
                                              &verify_result);
  }

  int Verify() {
    return VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor());
  }

  int VerifyAsTrustedSelfSignedLeaf() {
    return VerifyWithTrust(
        bssl::CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned());
  }

  std::unique_ptr<CertBuilder> cert_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcConstraintsTrustedSelfSignedTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, BaseCase) {
  // Without changing anything on the test cert, it should validate
  // successfully. If this is not true then the rest of the tests in this class
  // are unlikely to be useful.
  if (VerifyProcTypeIsBuiltin()) {
    // Should succeed when verified as a trusted leaf.
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()),
                IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()),
                IsOk());

    // Should also be allowed by verifying as anchor for itself.
    EXPECT_THAT(Verify(), IsOk());

    // Should fail if verified as anchor of itself with constraints enabled,
    // enforcing the basicConstraints on the anchor will fail since the cert
    // has CA=false.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor()
                                    .WithEnforceAnchorConstraints()),
                IsError(ERR_CERT_INVALID));

    // Should be allowed since it will be evaluated as a trusted leaf, so
    // anchor constraints being enabled doesn't matter.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                    .WithEnforceAnchorConstraints()),
                IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, BasicConstraintsIsCa) {
  for (bool has_key_usage_cert_sign : {false, true}) {
    cert_->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/-1);

    if (has_key_usage_cert_sign) {
      cert_->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                           bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    } else {
      cert_->SetKeyUsages({bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});
    }
    EXPECT_THAT(Verify(), IsOk());
    if (VerifyProcTypeIsBuiltin()) {
      EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
    }
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       BasicConstraintsNotCaPathlen) {
  cert_->SetBasicConstraints(/*is_ca=*/false, /*path_len=*/0);

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       BasicConstraintsIsCaPathlen) {
  cert_->SetBasicConstraints(/*is_ca=*/true, /*path_len=*/0);

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       BasicConstraintsMissing) {
  cert_->EraseExtension(bssl::der::Input(bssl::kBasicConstraintsOid));

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       NameConstraintsNotMatching) {
  cert_->SetNameConstraintsDnsNames(/*permitted_dns_names=*/{"example.org"},
                                    /*excluded_dns_names=*/{});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, ValidityExpired) {
  cert_->SetValidity(base::Time::Now() - base::Days(14),
                     base::Time::Now() - base::Days(7));

  EXPECT_THAT(Verify(), IsError(ERR_CERT_DATE_INVALID));
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(),
                IsError(ERR_CERT_DATE_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, PolicyConstraints) {
  static const char kPolicy1[] = "1.2.3.4";

  for (bool leaf_has_policy : {false, true}) {
    SCOPED_TRACE(leaf_has_policy);

    cert_->SetPolicyConstraints(
        /*require_explicit_policy=*/0,
        /*inhibit_policy_mapping=*/std::nullopt);
    if (leaf_has_policy) {
      cert_->SetCertificatePolicies({kPolicy1});

      EXPECT_THAT(Verify(), IsOk());
    } else {
      cert_->SetCertificatePolicies({});

      if (VerifyProcTypeIsBuiltin()) {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
        EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
      } else {
        EXPECT_THAT(Verify(), IsOk());
      }
    }
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, InhibitAnyPolicy) {
  static const char kAnyPolicy[] = "2.5.29.32.0";
  cert_->SetPolicyConstraints(
      /*require_explicit_policy=*/0,
      /*inhibit_policy_mapping=*/std::nullopt);
  cert_->SetInhibitAnyPolicy(0);
  cert_->SetCertificatePolicies({kAnyPolicy});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       KeyUsageNoDigitalSignature) {
  // This test is mostly uninteresting since keyUsage on the end-entity is only
  // checked at the TLS layer, not during cert verification.
  cert_->SetKeyUsages({bssl::KEY_USAGE_BIT_CRL_SIGN});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, KeyUsageCertSignLeaf) {
  // Test a leaf that has keyUsage asserting keyCertSign with basicConstraints
  // CA=false, which is an error according to 5280 (4.2.1.3 and 4.2.1.9).
  cert_->SetKeyUsages({bssl::KEY_USAGE_BIT_KEY_CERT_SIGN,
                       bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE});

  EXPECT_THAT(Verify(), IsOk());
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchor()
                                    .WithEnforceAnchorConstraints()),
                IsError(ERR_CERT_INVALID));
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                    .WithEnforceAnchorConstraints()
                                    .WithRequireLeafSelfSigned()),
                IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       ExtendedKeyUsageNoServerAuth) {
  cert_->SetExtendedKeyUsages({bssl::der::Input(bssl::kCodeSigning)});

  EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
  if (VerifyProcTypeIsBuiltin()) {
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsError(ERR_CERT_INVALID));
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest,
       UnknownSignatureAlgorithm) {
  cert_->SetSignatureAlgorithmTLV(TestOid0SignatureAlgorithmTLV());
  if (VerifyProcTypeIsBuiltin()) {
    // Attempts to verify as anchor of itself, which fails when verifying the
    // signature.
    EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));

    // Signature not checked when verified as a directly trusted leaf without
    // require_leaf_selfsigned.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()),
                IsOk());

    // PathBuilder override ignores require_leaf_selfsigned due to the
    // self-signed check returning false (due to the invalid signature
    // algorithm), thus this fails with AUTHORITY_INVALID due to failing to
    // find a chain to another root.
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(),
                IsError(ERR_CERT_AUTHORITY_INVALID));

    // PathBuilder override ignores require_leaf_selfsigned due to the invalid
    // signature algorithm, thus this tries to verify as anchor of itself,
    // which fails when verifying the signature.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                    .WithRequireLeafSelfSigned()),
                IsError(ERR_CERT_INVALID));
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, WeakSignatureAlgorithm) {
  cert_->SetSignatureAlgorithm(bssl::SignatureAlgorithm::kEcdsaSha1);
  if (VerifyProcTypeIsBuiltin()) {
    // Attempts to verify as anchor of itself, which fails due to the weak
    // signature algorithm.
    EXPECT_THAT(Verify(), IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));

    // Signature not checked when verified as a directly trusted leaf without
    // require_leaf_selfsigned.
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustedLeaf()),
                IsOk());

    // require_leaf_selfsigned allows any supported signature algorithm when
    // doing the self-signed check, so this is okay.
    EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
    EXPECT_THAT(VerifyWithTrust(bssl::CertificateTrust::ForTrustAnchorOrLeaf()
                                    .WithRequireLeafSelfSigned()),
                IsOk());
  } else {
    EXPECT_THAT(Verify(), IsOk());
  }
}

TEST_P(CertVerifyProcConstraintsTrustedSelfSignedTest, UnknownExtension) {
  for (bool critical : {true, false}) {
    SCOPED_TRACE(critical);
    cert_->SetExtension(TestOid0(), "hello world", critical);

    if (VerifyProcTypeIsBuiltin()) {
      if (critical) {
        EXPECT_THAT(Verify(), IsError(ERR_CERT_INVALID));
        EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsError(ERR_CERT_INVALID));
      } else {
        EXPECT_THAT(Verify(), IsOk());
        EXPECT_THAT(VerifyAsTrustedSelfSignedLeaf(), IsOk());
      }
    } else {
      EXPECT_THAT(Verify(), IsOk());
    }
  }
}

TEST(CertVerifyProcTest, RejectsPublicSHA1) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_sha1 = true;
  result.is_issued_by_known_root = true;
  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);

  // VERIFY_ENABLE_SHA1_LOCAL_ANCHORS should not impact this.
  flags = CertVerifyProc::VERIFY_ENABLE_SHA1_LOCAL_ANCHORS;
  verify_result.Reset();
  error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
}

TEST(CertVerifyProcTest, RejectsPrivateSHA1UnlessFlag) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_sha1 = true;
  result.is_issued_by_known_root = false;
  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  // SHA-1 should be rejected by default for private roots...
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);

  // ... unless VERIFY_ENABLE_SHA1_LOCAL_ANCHORS was supplied.
  flags = CertVerifyProc::VERIFY_ENABLE_SHA1_LOCAL_ANCHORS;
  verify_result.Reset();
  error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
}

enum ExpectedAlgorithms {
  EXPECT_SHA1 = 1 << 0,
  EXPECT_STATUS_INVALID = 1 << 1,
};

struct WeakDigestTestData {
  const char* root_cert_filename;
  const char* intermediate_cert_filename;
  const char* ee_cert_filename;
  int expected_algorithms;
};

const char* StringOrDefault(const char* str, const char* default_value) {
  if (!str)
    return default_value;
  return str;
}

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const WeakDigestTestData& data, std::ostream* os) {
  *os << "root: " << StringOrDefault(data.root_cert_filename, "none")
      << "; intermediate: "
      << StringOrDefault(data.intermediate_cert_filename, "none")
      << "; end-entity: " << data.ee_cert_filename;
}

class CertVerifyProcWeakDigestTest
    : public testing::TestWithParam<WeakDigestTestData> {
 public:
  CertVerifyProcWeakDigestTest() = default;
  ~CertVerifyProcWeakDigestTest() override = default;
};

// Tests that the CertVerifyProc::Verify() properly surfaces the (weak) hash
// algorithms used in the chain.
TEST_P(CertVerifyProcWeakDigestTest, VerifyDetectsAlgorithm) {
  WeakDigestTestData data = GetParam();
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Build |intermediates| as the full chain (including trust anchor).
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;

  if (data.intermediate_cert_filename) {
    scoped_refptr<X509Certificate> intermediate_cert =
        ImportCertFromFile(certs_dir, data.intermediate_cert_filename);
    ASSERT_TRUE(intermediate_cert);
    intermediates.push_back(bssl::UpRef(intermediate_cert->cert_buffer()));
  }

  if (data.root_cert_filename) {
    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(certs_dir, data.root_cert_filename);
    ASSERT_TRUE(root_cert);
    intermediates.push_back(bssl::UpRef(root_cert->cert_buffer()));
  }

  scoped_refptr<X509Certificate> ee_cert =
      ImportCertFromFile(certs_dir, data.ee_cert_filename);
  ASSERT_TRUE(ee_cert);

  scoped_refptr<X509Certificate> ee_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(ee_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(ee_chain);

  int flags = 0;
  CertVerifyResult verify_result;

  // Use a mock CertVerifyProc that returns success with a verified_cert of
  // |ee_chain|.
  //
  // This is sufficient for the purposes of this test, as the checking for weak
  // hash algorithms is done by CertVerifyProc::Verify().
  auto proc = base::MakeRefCounted<MockCertVerifyProc>(CertVerifyResult());
  int error = proc->Verify(ee_chain.get(), "127.0.0.1",
                           /*ocsp_response=*/std::string(),
                           /*sct_list=*/std::string(), flags, &verify_result,
                           NetLogWithSource());
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_SHA1), verify_result.has_sha1);
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_STATUS_INVALID),
            !!(verify_result.cert_status & CERT_STATUS_INVALID));
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_STATUS_INVALID),
            error == ERR_CERT_INVALID);
}

// The signature algorithm of the root CA should not matter.
const WeakDigestTestData kVerifyRootCATestData[] = {
    {"weak_digest_md5_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1},
    {"weak_digest_md4_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1},
    {"weak_digest_md2_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1},
};
INSTANTIATE_TEST_SUITE_P(VerifyRoot,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyRootCATestData));

// The signature algorithm of intermediates should be properly detected.
const WeakDigestTestData kVerifyIntermediateCATestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_STATUS_INVALID | EXPECT_SHA1},
    {"weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_STATUS_INVALID | EXPECT_SHA1},
    {"weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_STATUS_INVALID | EXPECT_SHA1},
};

INSTANTIATE_TEST_SUITE_P(VerifyIntermediate,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyIntermediateCATestData));

// The signature algorithm of end-entity should be properly detected.
const WeakDigestTestData kVerifyEndEntityTestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md5_ee.pem", EXPECT_STATUS_INVALID},
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md4_ee.pem", EXPECT_STATUS_INVALID},
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_STATUS_INVALID},
};

INSTANTIATE_TEST_SUITE_P(VerifyEndEntity,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyEndEntityTestData));

// Incomplete chains do not report the status of the intermediate.
// Note: really each of these tests should also expect the digest algorithm of
// the intermediate (included as a comment). However CertVerifyProc::Verify() is
// unable to distinguish that this is an intermediate and not a trust anchor, so
// this intermediate is treated like a trust anchor.
const WeakDigestTestData kVerifyIncompleteIntermediateTestData[] = {
    {nullptr, "weak_digest_md5_intermediate.pem", "weak_digest_sha1_ee.pem",
     EXPECT_SHA1},
    {nullptr, "weak_digest_md4_intermediate.pem", "weak_digest_sha1_ee.pem",
     EXPECT_SHA1},
    {nullptr, "weak_digest_md2_intermediate.pem", "weak_digest_sha1_ee.pem",
     EXPECT_SHA1},
};

INSTANTIATE_TEST_SUITE_P(
    MAYBE_VerifyIncompleteIntermediate,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteIntermediateTestData));

// Incomplete chains should report the status of the end-entity.
// since the intermediate is treated as a trust anchor these should
// be still simply be invalid.
const WeakDigestTestData kVerifyIncompleteEETestData[] = {
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md5_ee.pem",
     EXPECT_STATUS_INVALID},
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md4_ee.pem",
     EXPECT_STATUS_INVALID},
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md2_ee.pem",
     EXPECT_STATUS_INVALID},
};

INSTANTIATE_TEST_SUITE_P(VerifyIncompleteEndEntity,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyIncompleteEETestData));

// Md2, Md4, and Md5 are all considered invalid.
const WeakDigestTestData kVerifyMixedTestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_STATUS_INVALID},
    {"weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
     "weak_digest_md5_ee.pem", EXPECT_STATUS_INVALID},
    {"weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_STATUS_INVALID},
};

INSTANTIATE_TEST_SUITE_P(VerifyMixed,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyMixedTestData));

// The EE is a trusted certificate. Even though it uses weak hashes, these
// should not be reported.
const WeakDigestTestData kVerifyTrustedEETestData[] = {
    {nullptr, nullptr, "weak_digest_md5_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_md4_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_md2_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_sha1_ee.pem", 0},
};

INSTANTIATE_TEST_SUITE_P(VerifyTrustedEE,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyTrustedEETestData));

// Test fixture for verifying certificate names.
class CertVerifyProcNameTest : public ::testing::Test {
 protected:
  void VerifyCertName(const char* hostname, bool valid) {
    scoped_refptr<X509Certificate> cert(ImportCertFromFile(
        GetTestCertsDirectory(), "subjectAltName_sanity_check.pem"));
    ASSERT_TRUE(cert);
    CertVerifyResult result;
    result.is_issued_by_known_root = false;
    auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

    CertVerifyResult verify_result;
    int error = verify_proc->Verify(
        cert.get(), hostname, /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, &verify_result, NetLogWithSource());
    if (valid) {
      EXPECT_THAT(error, IsOk());
      EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
    } else {
      EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
      EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
    }
  }
};

// Don't match the common name
TEST_F(CertVerifyProcNameTest, DontMatchCommonName) {
  VerifyCertName("127.0.0.1", false);
}

// Matches the iPAddress SAN (IPv4)
TEST_F(CertVerifyProcNameTest, MatchesIpSanIpv4) {
  VerifyCertName("127.0.0.2", true);
}

// Matches the iPAddress SAN (IPv6)
TEST_F(CertVerifyProcNameTest, MatchesIpSanIpv6) {
  VerifyCertName("FE80:0:0:0:0:0:0:1", true);
}

// Should not match the iPAddress SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchIpSanIpv6) {
  VerifyCertName("[FE80:0:0:0:0:0:0:1]", false);
}

// Compressed form matches the iPAddress SAN (IPv6)
TEST_F(CertVerifyProcNameTest, MatchesIpSanCompressedIpv6) {
  VerifyCertName("FE80::1", true);
}

// IPv6 mapped form should NOT match iPAddress SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchIpSanIPv6Mapped) {
  VerifyCertName("::127.0.0.2", false);
}

// Matches the dNSName SAN
TEST_F(CertVerifyProcNameTest, MatchesDnsSan) {
  VerifyCertName("test.example", true);
}

// Matches the dNSName SAN (trailing . ignored)
TEST_F(CertVerifyProcNameTest, MatchesDnsSanTrailingDot) {
  VerifyCertName("test.example.", true);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSan) {
  VerifyCertName("www.test.example", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanInvalid) {
  VerifyCertName("test..example", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanTwoTrailingDots) {
  VerifyCertName("test.example..", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanLeadingAndTrailingDot) {
  VerifyCertName(".test.example.", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanTrailingDot) {
  VerifyCertName(".test.example", false);
}

// Test that trust anchors are appropriately recorded via UMA.
TEST(CertVerifyProcTest, HasTrustAnchorVerifyUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;

  // Simulate a certificate chain issued by "C=US, O=Google Trust Services LLC,
  // CN=GTS Root R4". This publicly-trusted root was chosen as it was included
  // in 2017 and is not anticipated to be removed from all supported platforms
  // for a few decades.
  // Note: The actual cert in |cert| does not matter for this testing, so long
  // as it's not violating any CertVerifyProc::Verify() policies.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue root_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(root_hash));

  const base::HistogramBase::Sample kGTSRootR4HistogramID = 486;

  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kGTSRootR4HistogramID, 1);
}

// Test that certificates with multiple trust anchors present result in
// only a single trust anchor being recorded, and that being the most specific
// trust anchor.
TEST(CertVerifyProcTest, LogsOnlyMostSpecificTrustAnchorUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;

  // Simulate a chain of "C=US, O=Google Trust Services LLC, CN=GTS Root R4"
  // signing "C=US, O=Google Trust Services LLC, CN=GTS Root R3" signing an
  // intermediate and a leaf.
  // Note: The actual cert in |cert| does not matter for this testing, so long
  // as it's not violating any CertVerifyProc::Verify() policies.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue gts_root_r3_hash = {
      {0x41, 0x79, 0xed, 0xd9, 0x81, 0xef, 0x74, 0x74, 0x77, 0xb4, 0x96,
       0x26, 0x40, 0x8a, 0xf4, 0x3d, 0xaa, 0x2c, 0xa7, 0xab, 0x7f, 0x9e,
       0x08, 0x2c, 0x10, 0x60, 0xf8, 0x40, 0x96, 0x77, 0x43, 0x48}};
  SHA256HashValue gts_root_r4_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(gts_root_r3_hash));
  result.public_key_hashes.push_back(HashValue(gts_root_r4_hash));

  const base::HistogramBase::Sample kGTSRootR3HistogramID = 485;

  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  // Only GTS Root R3 should be recorded.
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kGTSRootR3HistogramID, 1);
}

// Test that trust anchors histograms record whether or not
// is_issued_by_known_root was derived from the OS.
TEST(CertVerifyProcTest, HasTrustAnchorVerifyOutOfDateUMA) {
  base::HistogramTester histograms;
  // Since we are setting is_issued_by_known_root=true, the certificate to be
  // verified needs to have a validity period that satisfies
  // HasTooLongValidity.
  auto [leaf, root] = CertBuilder::CreateSimpleChain2();

  CertVerifyResult result;

  // Simulate a certificate chain that is recognized as trusted (from a known
  // root), but no certificates in the chain are tracked as known trust
  // anchors.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue root_hash = {{2}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(root_hash));
  result.is_issued_by_known_root = true;

  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);
  histograms.ExpectTotalCount(kTrustAnchorVerifyOutOfDateHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      leaf->GetX509Certificate().get(), "www.example.com",
      /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  const base::HistogramBase::Sample kUnknownRootHistogramID = 0;
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kUnknownRootHistogramID, 1);
  histograms.ExpectUniqueSample(kTrustAnchorVerifyOutOfDateHistogram, true, 1);
}

// If the CertVerifyProc::VerifyInternal implementation calculated the stapled
// OCSP results in the CertVerifyResult, CertVerifyProc::Verify should not
// re-calculate them.
TEST(CertVerifyProcTest, DoesNotRecalculateStapledOCSPResult) {
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(1U, cert->intermediate_buffers().size());

  CertVerifyResult result;

  result.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
  result.ocsp_result.revocation_status = bssl::OCSPRevocationStatus::GOOD;

  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(cert.get(), "127.0.0.1",
                                  /*ocsp_response=*/"invalid OCSP data",
                                  /*sct_list=*/std::string(), flags,
                                  &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  EXPECT_EQ(bssl::OCSPVerifyResult::PROVIDED,
            verify_result.ocsp_result.response_status);
  EXPECT_EQ(bssl::OCSPRevocationStatus::GOOD,
            verify_result.ocsp_result.revocation_status);
}

TEST(CertVerifyProcTest, CalculateStapledOCSPResultIfNotAlreadyDone) {
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(1U, cert->intermediate_buffers().size());

  CertVerifyResult result;

  // Confirm the default-constructed values are as expected.
  EXPECT_EQ(bssl::OCSPVerifyResult::NOT_CHECKED,
            result.ocsp_result.response_status);
  EXPECT_EQ(bssl::OCSPRevocationStatus::UNKNOWN,
            result.ocsp_result.revocation_status);

  auto verify_proc = base::MakeRefCounted<MockCertVerifyProc>(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/"invalid OCSP data",
      /*sct_list=*/std::string(), flags, &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  EXPECT_EQ(bssl::OCSPVerifyResult::PARSE_RESPONSE_ERROR,
            verify_result.ocsp_result.response_status);
  EXPECT_EQ(bssl::OCSPRevocationStatus::UNKNOWN,
            verify_result.ocsp_result.revocation_status);
}

}  // namespace net
