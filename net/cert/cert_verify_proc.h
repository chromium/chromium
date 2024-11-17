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
#include "components/network_time/time_tracker/time_tracker.h"
#include "crypto/crypto_buildflags.h"
#include "net/base/hash_value.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/net_buildflags.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

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

  // The set factory parameters that are variable over time, but are expected to
  // be consistent between multiple verifiers that are created. For example,
  // CertNetFetcher is not in this struct as it is expected that different
  // verifiers will have different net fetchers. (There is no technical
  // restriction against creating different verifiers with different ImplParams,
  // structuring the parameters this way just makes some APIs more convenient
  // for the common case.)
  struct NET_EXPORT ImplParams {
    ImplParams();
    ~ImplParams();
    ImplParams(const ImplParams&);
    ImplParams& operator=(const ImplParams& other);
    ImplParams(ImplParams&&);
    ImplParams& operator=(ImplParams&& other);

    scoped_refptr<CRLSet> crl_set;
    std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
    scoped_refptr<net::CTPolicyEnforcer> ct_policy_enforcer;
    std::optional<network_time::TimeTracker> time_tracker;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    std::optional<net::ChromeRootStoreData> root_store_data;
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    bool use_chrome_root_store;
#endif
  };

  // CIDR, consisting of an IP and a netmask.
  struct NET_EXPORT CIDR {
    net::IPAddress ip;
    net::IPAddress mask;
  };

  // Single certificate, with constraints.
  struct NET_EXPORT CertificateWithConstraints {
    CertificateWithConstraints();
    ~CertificateWithConstraints();
    CertificateWithConstraints(const CertificateWithConstraints&);
    CertificateWithConstraints& operator=(
        const CertificateWithConstraints& other);
    CertificateWithConstraints(CertificateWithConstraints&&);
    CertificateWithConstraints& operator=(CertificateWithConstraints&& other);

    std::shared_ptr<const bssl::ParsedCertificate> certificate;

    std::vector<std::string> permitted_dns_names;

    std::vector<CIDR> permitted_cidrs;
  };

  // The set of parameters that are variable over time and can differ between
  // different verifiers created by a CertVerifierProcFactory.
  struct NET_EXPORT InstanceParams {
    InstanceParams();
    ~InstanceParams();
    InstanceParams(const InstanceParams&);
    InstanceParams& operator=(const InstanceParams& other);
    InstanceParams(InstanceParams&&);
    InstanceParams& operator=(InstanceParams&& other);

    // Additional trust anchors to consider during path validation. Ordinarily,
    // implementations of CertVerifier use trust anchors from the configured
    // system store. This is implementation-specific plumbing for passing
    // additional anchors through.
    bssl::ParsedCertificateList additional_trust_anchors;

    // Same as additional_trust_anchors, but embedded anchor constraints and
    // NotBefore/NotAfter are enforced.
    bssl::ParsedCertificateList
        additional_trust_anchors_with_enforced_constraints;

    // Additional trust anchors to consider during path validation, but with
    // name constraints specified outside of the certificate.
    std::vector<CertificateWithConstraints>
        additional_trust_anchors_with_constraints;

    // Additional trust leafs to consider during path validation, possibly with
    // name constraints specified outside of the certificate.
    std::vector<CertificateWithConstraints> additional_trust_leafs;

    // Additional trust anchors/leafs to consider during path validation,
    // possibly with name constraints specified outside of the certificate.
    std::vector<CertificateWithConstraints> additional_trust_anchors_and_leafs;

    // Additional temporary certs to consider as intermediates during path
    // validation. Ordinarily, implementations of CertVerifier use intermediate
    // certs from the configured system store. This is implementation-specific
    // plumbing for passing additional intermediates through.
    bssl::ParsedCertificateList additional_untrusted_authorities;

    //  Additional SPKIs to consider as distrusted during path validation.
    std::vector<std::vector<uint8_t>> additional_distrusted_spkis;

#if !BUILDFLAG(IS_CHROMEOS)
    // If true, use the user-added certs in the system trust store for path
    // validation.
    // This only has an impact if the Chrome Root Store is being used.
    bool include_system_trust_store = true;
#endif
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

#if BUILDFLAG(IS_FUCHSIA)
  // Creates and returns a CertVerifyProcBuiltin using the SSL SystemTrustStore.
  static scoped_refptr<CertVerifyProc> CreateBuiltinVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set,
      std::unique_ptr<CTVerifier> ct_verifier,
      scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
      const InstanceParams instance_params,
      std::optional<network_time::TimeTracker> time_tracker);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Creates and returns a CertVerifyProcBuiltin using the Chrome Root Store
  // SystemTrustStore and the given |root_store_data|, which may be nullptr to
  // use the default.
  static scoped_refptr<CertVerifyProc> CreateBuiltinWithChromeRootStore(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      scoped_refptr<CRLSet> crl_set,
      std::unique_ptr<CTVerifier> ct_verifier,
      scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
      const ChromeRootStoreData* root_store_data,
      const InstanceParams instance_params,
      std::optional<network_time::TimeTracker> time_tracker);
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
  // If |time_now| is set it will be used as the current time, otherwise the
  // system time will be used.
  //
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, online certificate
  // revocation checking is performed (i.e. OCSP and downloading CRLs). CRLSet
  // based revocation checking is always enabled, regardless of this flag.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             const std::string& ocsp_response,
             const std::string& sct_list,
             int flags,
             CertVerifyResult* verify_result,
             const NetLogWithSource& net_log);

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
  // If |time_now| is not nullopt, it will be used as the current time for
  // certificate verification, if it is nullopt, the system time will be used
  // instead. If a certificate verification fails with a NotBefore/NotAfter
  // error when |time_now| is set, it will be retried with the system time.
  //
  // On success, net::OK should be returned, with |verify_result| updated to
  // reflect the successfully verified chain.
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             const std::string& ocsp_response,
                             const std::string& sct_list,
                             int flags,
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

  // Checks the validity period of the certificate against the maximum
  // allowable validity period for publicly trusted certificates. Returns true
  // if the validity period is too long.
  static bool HasTooLongValidity(const X509Certificate& cert);

  const scoped_refptr<CRLSet> crl_set_;
};

// Factory for creating new CertVerifyProcs when they need to be updated.
class NET_EXPORT CertVerifyProcFactory
    : public base::RefCountedThreadSafe<CertVerifyProcFactory> {
 public:

  // Create a new CertVerifyProc that uses the passed in CRLSet and
  // ChromeRootStoreData.
  virtual scoped_refptr<CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const CertVerifyProc::ImplParams& impl_params,
      const CertVerifyProc::InstanceParams& instance_params) = 0;

 protected:
  virtual ~CertVerifyProcFactory() = default;

 private:
  friend class base::RefCountedThreadSafe<CertVerifyProcFactory>;
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_H_
