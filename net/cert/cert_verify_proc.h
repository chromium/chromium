// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_H_
#define NET_CERT_CERT_VERIFY_PROC_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif

namespace net {

class CertNetFetcher;
class CertVerifyResult;
class CRLSet;
class NetLogWithSource;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;

// Class to perform certificate path building and verification for various
// certificate uses. All methods of this class must be thread-safe, as they
// may be called from various non-joinable worker threads.
class NET_EXPORT CertVerifyProc
    : public base::RefCountedThreadSafe<CertVerifyProc> {
 public:
  enum VerifyFlags {
    // If set, enables online revocation checking via CRLs and OCSP for the
    // certificate chain.
    // Note: has no effect if VERIFY_DISABLE_NETWORK_FETCHES is set.
    VERIFY_REV_CHECKING_ENABLED = 1 << 0,

    // If set, this is equivalent to VERIFY_REV_CHECKING_ENABLED, in that it
    // enables online revocation checking via CRLs or OCSP, but only
    // for certificates issued by non-public trust anchors. Failure to check
    // revocation is treated as a hard failure.
    // Note: has no effect if VERIFY_DISABLE_NETWORK_FETCHES is set.
    VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS = 1 << 1,

    // If set, certificates with SHA-1 signatures will be allowed, but only if
    // they are issued by non-public trust anchors.
    VERIFY_ENABLE_SHA1_LOCAL_ANCHORS = 1 << 2,

    // If set, disables the policy enforcement described at
    // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
    VERIFY_DISABLE_SYMANTEC_ENFORCEMENT = 1 << 3,

    // Disable network fetches during verification. This will override
    // VERIFY_REV_CHECKING_ENABLED and
    // VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS if they are also specified.
    // (Note that this entirely disables the online revocation/AIA code paths.
    // Theoretically we could still check for cached results.)
    VERIFY_DISABLE_NETWORK_FETCHES = 1 << 4,

    // Also update GetNetConstants() in net/log/net_log_util.cc when updating
    // this enum.
    VERIFY_FLAGS_LAST = VERIFY_DISABLE_NETWORK_FETCHES
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

#if !(BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || \
      BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(CHROME_ROOT_STORE_ONLY))
  // Creates and returns a CertVerifyProc that uses the system verifier.
  // |cert_net_fetcher| may not be used, depending on the implementation.
  static scoped_refptr<CertVerifyProc> CreateSystemVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set);
#endif

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(USE_NSS_CERTS)
  // Creates and returns a CertVerifyProcBuiltin using the SSL SystemTrustStore.
  static scoped_refptr<CertVerifyProc> CreateBuiltinVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Creates and returns a CertVerifyProcBuiltin using the Chrome Root Store
  // SystemTrustStore and the given |root_store_data|, which may be nullptr to
  // use the default.
  static scoped_refptr<CertVerifyProc> CreateBuiltinWithChromeRootStore(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set,
      const ChromeRootStoreData* root_store_data);
#endif

  CertVerifyProc(const CertVerifyProc&) = delete;
  CertVerifyProc& operator=(const CertVerifyProc&) = delete;

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
  // based revocation checking is always enabled, regardless of this flag.
  //
  // |additional_trust_anchors| lists certificates that can be trusted when
  // building a certificate chain, in addition to the anchors known to the
  // implementation.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             const std::string& ocsp_response,
             const std::string& sct_list,
             int flags,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result,
             const NetLogWithSource& net_log);

  // Returns true if the implementation supports passing additional trust
  // anchors to the Verify() call. The |additional_trust_anchors| parameter
  // passed to Verify() is ignored when this returns false.
  virtual bool SupportsAdditionalTrustAnchors() const = 0;

 protected:
  explicit CertVerifyProc(scoped_refptr<CRLSet> crl_set);
  virtual ~CertVerifyProc();

  CRLSet* crl_set() const { return crl_set_.get(); }

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
  // * has_sha1
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
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result,
                             const NetLogWithSource& net_log) = 0;

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

  const scoped_refptr<CRLSet> crl_set_;
};

// Factory for creating new CertVerifyProcs when they need to be updated.
class NET_EXPORT CertVerifyProcFactory
    : public base::RefCountedThreadSafe<CertVerifyProcFactory> {
 public:
  // The set of factory parameters that are variable over time, but are
  // expected to be consistent between multiple verifiers that are created. For
  // example, CertNetFetcher is not in this struct as it is expected that
  // different verifiers will have different net fetchers. (There is no
  // technical restriction against creating different verifiers with different
  // ImplParams, structuring the parameters this way just makes some APIs more
  // convenient for the common case.)
  struct NET_EXPORT ImplParams {
    ImplParams();
    ~ImplParams();
    ImplParams(const ImplParams&);
    ImplParams& operator=(const ImplParams& other);
    ImplParams(ImplParams&&);
    ImplParams& operator=(ImplParams&& other);

    scoped_refptr<CRLSet> crl_set;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    absl::optional<net::ChromeRootStoreData> root_store_data;
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    bool use_chrome_root_store;
#endif
  };

  // Create a new CertVerifyProc that uses the passed in CRLSet and
  // ChromeRootStoreData.
  virtual scoped_refptr<CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const ImplParams& impl_params) = 0;

 protected:
  virtual ~CertVerifyProcFactory() = default;

 private:
  friend class base::RefCountedThreadSafe<CertVerifyProcFactory>;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_H_
