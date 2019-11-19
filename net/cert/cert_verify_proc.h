// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_H_
#define NET_CERT_CERT_VERIFY_PROC_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/cert/x509_cert_types.h"

namespace net {

class CertNetFetcher;
class CertVerifyResult;
class CRLSet;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// Class to perform certificate path building and verification for various
// certificate uses. All methods of this class must be thread-safe, as they
// may be called from various non-joinable worker threads.
class NET_EXPORT CertVerifyProc
    : public base::RefCountedThreadSafe<CertVerifyProc> {
 public:
  enum VerifyFlags {
    // If set, enables online revocation checking via CRLs and OCSP for the
    // certificate chain.
    VERIFY_REV_CHECKING_ENABLED = 1 << 0,

    // If set, this is equivalent to VERIFY_REV_CHECKING_ENABLED, in that it
    // enables online revocation checking via CRLs or OCSP, but only
    // for certificates issued by non-public trust anchors. Failure to check
    // revocation is treated as a hard failure.
    // Note: If VERIFY_CERT_IO_ENABLE is not also supplied, certificates
    // that chain to local trust anchors will likely fail - for example, due to
    // lacking fresh cached revocation issue (Windows) or because OCSP stapling
    // can only provide information for the leaf, and not for any
    // intermediates.
    VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS = 1 << 1,

    // If set, certificates with SHA-1 signatures will be allowed, but only if
    // they are issued by non-public trust anchors.
    VERIFY_ENABLE_SHA1_LOCAL_ANCHORS = 1 << 2,

    // If set, disables the policy enforcement described at
    // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
    VERIFY_DISABLE_SYMANTEC_ENFORCEMENT = 1 << 3,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NameNormalizationResult {
    kError = 0,
    kByteEqual = 1,
    kNormalized = 2,
    kChainLengthOne = 3,
    kMaxValue = kChainLengthOne
  };

#if !defined(OS_FUCHSIA)
  // Creates and returns a CertVerifyProc that uses the system verifier.
  // |cert_net_fetcher| may not be used, depending on the implementation.
  static scoped_refptr<CertVerifyProc> CreateSystemVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher);
#endif

#if defined(OS_FUCHSIA) || defined(USE_NSS_CERTS) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
  // Creates and returns a CertVerifyProcBuiltin using the SSL SystemTrustStore.
  static scoped_refptr<CertVerifyProc> CreateBuiltinVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher);
#endif

  // Verifies the certificate against the given hostname as an SSL server
  // certificate. Returns OK if successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value. If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |ocsp_response|, if non-empty, is a stapled OCSP response to use.
  //
  // |sct_list|, if non-empty, is a SignedCertificateTimestampList from the TLS
  // extension as described in RFC6962 section 3.3.1.
  //
  // |flags| is bitwise OR'd of VerifyFlags:
  //
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, online certificate
  // revocation checking is performed (i.e. OCSP and downloading CRLs). CRLSet
  // based revocation checking is always enabled, regardless of this flag, if
  // |crl_set| is given.
  //
  // |crl_set|, which is required, points to an CRLSet structure which can be
  // used to avoid revocation checks over the network.  If you do not have one
  // handy, use CRLSet::BuiltinCRLSet().
  //
  // |additional_trust_anchors| lists certificates that can be trusted when
  // building a certificate chain, in addition to the anchors known to the
  // implementation.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             const std::string& ocsp_response,
             const std::string& sct_list,
             int flags,
             CRLSet* crl_set,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result);

  // Returns true if the implementation supports passing additional trust
  // anchors to the Verify() call. The |additional_trust_anchors| parameter
  // passed to Verify() is ignored when this returns false.
  virtual bool SupportsAdditionalTrustAnchors() const = 0;

 protected:
  CertVerifyProc();
  virtual ~CertVerifyProc();

  // Record a histogram of whether Name normalization was used in verifying the
  // chain. This should only be called for successfully validated chains.
  static void LogNameNormalizationResult(const std::string& histogram_suffix,
                                         NameNormalizationResult result);

  // Record a histogram of whether Name normalization was used in verifying the
  // chain. This should only be called for successfully validated chains.
  static void LogNameNormalizationMetrics(const std::string& histogram_suffix,
                                          X509Certificate* verified_cert,
                                          bool is_issued_by_known_root);

 private:
  friend class base::RefCountedThreadSafe<CertVerifyProc>;
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest, DigiNotarCerts);
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest, TestHasTooLongValidity);
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest,
                           VerifyRejectsSHA1AfterDeprecationLegacyMode);
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest, SymantecCertsRejected);

  // Performs the actual verification using the desired underlying
  //
  // On entry, |verify_result| will be default-initialized as a successful
  // validation, with |verify_result->verified_cert| set to |cert|.
  //
  // Implementations are expected to fill in all applicable fields, excluding:
  //
  // * ocsp_result
  // * has_md2
  // * has_md4
  // * has_md5
  // * has_sha1
  // * has_sha1_leaf
  //
  // which will be filled in by |Verify()|. If an error code is returned,
  // |verify_result->cert_status| should be non-zero, indicating an
  // error occurred.
  //
  // On success, net::OK should be returned, with |verify_result| updated to
  // reflect the successfully verified chain.
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             const std::string& ocsp_response,
                             const std::string& sct_list,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) = 0;

  // HasNameConstraintsViolation returns true iff one of |public_key_hashes|
  // (which are hashes of SubjectPublicKeyInfo structures) has name constraints
  // imposed on it and the names in |dns_names| are not permitted.
  static bool HasNameConstraintsViolation(
      const HashValueVector& public_key_hashes,
      const std::string& common_name,
      const std::vector<std::string>& dns_names,
      const std::vector<std::string>& ip_addrs);

  // The CA/Browser Forum's Baseline Requirements specify maximum validity
  // periods (https://cabforum.org/baseline-requirements-documents/).
  //
  // For certificates issued after 1 July 2012: 60 months.
  // For certificates issued after 1 April 2015: 39 months.
  // For certificates issued after 1 March 2018: 825 days.
  //
  // For certificates issued before the BRs took effect, there were no
  // guidelines, but clamp them at a maximum of 10 year validity, with the
  // requirement they expire within 7 years after the effective date of the BRs
  // (i.e. by 1 July 2019).
  static bool HasTooLongValidity(const X509Certificate& cert);

  // Feature flag affecting the Legacy Symantec PKI deprecation, documented
  // at https://g.co/chrome/symantecpkicerts
  static const base::Feature kLegacySymantecPKIEnforcement;

  DISALLOW_COPY_AND_ASSIGN(CertVerifyProc);
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_H_
