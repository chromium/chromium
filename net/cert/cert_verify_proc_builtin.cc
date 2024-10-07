// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_issuer_source_aia.h"
#include "net/cert/internal/revocation_checker.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/cert_issuer_source_static.h"
#include "third_party/boringssl/src/pki/common_cert_errors.h"
#include "third_party/boringssl/src/pki/name_constraints.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/path_builder.h"
#include "third_party/boringssl/src/pki/simple_path_builder_delegate.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_collection.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "base/version_info/version_info.h"  // nogncheck
#include "net/cert/internal/trust_store_chrome.h"
#endif

using bssl::CertErrorId;

namespace net {

namespace {

// To avoid a denial-of-service risk, cap iterations by the path builder.
// Without a limit, path building is potentially exponential. This limit was
// set based on UMA histograms in the wild. See https://crrev.com/c/4903550.
//
// TODO(crbug.com/41267856): Move this limit into BoringSSL as a default.
constexpr uint32_t kPathBuilderIterationLimit = 20;

constexpr base::TimeDelta kMaxVerificationTime = base::Seconds(60);

constexpr base::TimeDelta kPerAttemptMinVerificationTimeLimit =
    base::Seconds(5);

DEFINE_CERT_ERROR_ID(kPathLacksEVPolicy, "Path does not have an EV policy");
DEFINE_CERT_ERROR_ID(kChromeRootConstraintsFailed,
                     "Path does not satisfy CRS constraints");

base::Value::Dict NetLogCertParams(const CRYPTO_BUFFER* cert_handle,
                                   const bssl::CertErrors& errors) {
  base::Value::Dict results;

  std::string pem_encoded;
  if (X509Certificate::GetPEMEncodedFromDER(
          x509_util::CryptoBufferAsStringPiece(cert_handle), &pem_encoded)) {
    results.Set("certificate", pem_encoded);
  }

  std::string errors_string = errors.ToDebugString();
  if (!errors_string.empty())
    results.Set("errors", errors_string);

  return results;
}

base::Value::Dict NetLogAdditionalCert(const CRYPTO_BUFFER* cert_handle,
                                       const bssl::CertificateTrust& trust,
                                       const bssl::CertErrors& errors) {
  base::Value::Dict results = NetLogCertParams(cert_handle, errors);
  results.Set("trust", trust.ToDebugString());
  return results;
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
base::Value::Dict NetLogChromeRootStoreVersion(
    int64_t chrome_root_store_version) {
  base::Value::Dict results;
  results.Set("version_major", NetLogNumberValue(chrome_root_store_version));
  return results;
}
#endif

base::Value::List PEMCertValueList(const bssl::ParsedCertificateList& certs) {
  base::Value::List value;
  for (const auto& cert : certs) {
    std::string pem;
    X509Certificate::GetPEMEncodedFromDER(cert->der_cert().AsStringView(),
                                          &pem);
    value.Append(std::move(pem));
  }
  return value;
}

base::Value::Dict NetLogPathBuilderResultPath(
    const bssl::CertPathBuilderResultPath& result_path) {
  base::Value::Dict dict;
  dict.Set("is_valid", result_path.IsValid());
  dict.Set("last_cert_trust", result_path.last_cert_trust.ToDebugString());
  dict.Set("certificates", PEMCertValueList(result_path.certs));
  // TODO(crbug.com/40479281): netlog user_constrained_policy_set.
  std::string errors_string =
      result_path.errors.ToDebugString(result_path.certs);
  if (!errors_string.empty())
    dict.Set("errors", errors_string);
  return dict;
}

base::Value::Dict NetLogPathBuilderResult(
    const bssl::CertPathBuilder::Result& result) {
  base::Value::Dict dict;
  // TODO(crbug.com/40479281): include debug data (or just have things netlog it
  // directly).
  dict.Set("has_valid_path", result.HasValidPath());
  dict.Set("best_result_index", static_cast<int>(result.best_result_index));
  if (result.exceeded_iteration_limit)
    dict.Set("exceeded_iteration_limit", true);
  if (result.exceeded_deadline)
    dict.Set("exceeded_deadline", true);
  return dict;
}

RevocationPolicy NoRevocationChecking() {
  RevocationPolicy policy;
  policy.check_revocation = false;
  policy.networking_allowed = false;
  policy.crl_allowed = false;
  policy.allow_missing_info = true;
  policy.allow_unable_to_check = true;
  policy.enforce_baseline_requirements = false;
  return policy;
}

// Gets the set of policy OIDs in |cert| that are recognized as EV OIDs for some
// root.
void GetEVPolicyOids(const EVRootCAMetadata* ev_metadata,
                     const bssl::ParsedCertificate* cert,
                     std::set<bssl::der::Input>* oids) {
  oids->clear();

  if (!cert->has_policy_oids())
    return;

  for (const bssl::der::Input& oid : cert->policy_oids()) {
    if (ev_metadata->IsEVPolicyOID(oid)) {
      oids->insert(oid);
    }
  }
}

// Returns true if |cert| could be an EV certificate, based on its policies
// extension. A return of false means it definitely is not an EV certificate,
// whereas a return of true means it could be EV.
bool IsEVCandidate(const EVRootCAMetadata* ev_metadata,
                   const bssl::ParsedCertificate* cert) {
  std::set<bssl::der::Input> oids;
  GetEVPolicyOids(ev_metadata, cert, &oids);
  return !oids.empty();
}

// CertVerifyProcTrustStore wraps a SystemTrustStore with additional trust
// anchors and TestRootCerts.
class CertVerifyProcTrustStore {
 public:
  // |system_trust_store| must outlive this object.
  explicit CertVerifyProcTrustStore(
      SystemTrustStore* system_trust_store,
      bssl::TrustStoreInMemory* additional_trust_store)
      : system_trust_store_(system_trust_store),
        additional_trust_store_(additional_trust_store) {
    trust_store_.AddTrustStore(additional_trust_store_);
    trust_store_.AddTrustStore(system_trust_store_->GetTrustStore());
    // When running in test mode, also layer in the test-only root certificates.
    //
    // Note that this integration requires TestRootCerts::HasInstance() to be
    // true by the time CertVerifyProcTrustStore is created - a limitation which
    // is acceptable for the test-only code that consumes this.
    if (TestRootCerts::HasInstance()) {
      trust_store_.AddTrustStore(
          TestRootCerts::GetInstance()->test_trust_store());
    }
  }

  bssl::TrustStore* trust_store() { return &trust_store_; }

  bool IsKnownRoot(const bssl::ParsedCertificate* trust_anchor) const {
    if (TestRootCerts::HasInstance() &&
        TestRootCerts::GetInstance()->IsKnownRoot(trust_anchor->der_cert())) {
      return true;
    }
    return system_trust_store_->IsKnownRoot(trust_anchor);
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  base::span<const ChromeRootCertConstraints> GetChromeRootConstraints(
      const bssl::ParsedCertificate* cert) const {
    return system_trust_store_->GetChromeRootConstraints(cert);
  }

  bool IsNonChromeRootStoreTrustAnchor(
      const bssl::ParsedCertificate* trust_anchor) const {
    return IsAdditionalTrustAnchor(trust_anchor) ||
           system_trust_store_->IsLocallyTrustedRoot(trust_anchor);
  }
#endif

  bool IsAdditionalTrustAnchor(
      const bssl::ParsedCertificate* trust_anchor) const {
    return additional_trust_store_->GetTrust(trust_anchor).IsTrustAnchor();
  }

 private:
  raw_ptr<SystemTrustStore> system_trust_store_;
  raw_ptr<bssl::TrustStoreInMemory> additional_trust_store_;
  bssl::TrustStoreCollection trust_store_;
};

// Enum for whether path building is attempting to verify a certificate as EV or
// as DV.
enum class VerificationType {
  kEV,  // Extended Validation
  kDV,  // Domain Validation
};

class PathBuilderDelegateDataImpl : public bssl::CertPathBuilderDelegateData {
 public:
  ~PathBuilderDelegateDataImpl() override = default;

  static const PathBuilderDelegateDataImpl* Get(
      const bssl::CertPathBuilderResultPath& path) {
    return static_cast<PathBuilderDelegateDataImpl*>(path.delegate_data.get());
  }

  static PathBuilderDelegateDataImpl* GetOrCreate(
      bssl::CertPathBuilderResultPath* path) {
    if (!path->delegate_data)
      path->delegate_data = std::make_unique<PathBuilderDelegateDataImpl>();
    return static_cast<PathBuilderDelegateDataImpl*>(path->delegate_data.get());
  }

  bssl::OCSPVerifyResult stapled_ocsp_verify_result;
  SignedCertificateTimestampAndStatusList scts;
  ct::CTPolicyCompliance ct_policy_compliance;
};

// TODO(eroman): The path building code in this file enforces its idea of weak
// keys, and signature algorithms, but separately cert_verify_proc.cc also
// checks the chains with its own policy. These policies must be aligned to
// give path building the best chance of finding a good path.
class PathBuilderDelegateImpl : public bssl::SimplePathBuilderDelegate {
 public:
  // Uses the default policy from bssl::SimplePathBuilderDelegate, which
  // requires RSA keys to be at least 1024-bits large, and optionally accepts
  // SHA1 certificates.
  PathBuilderDelegateImpl(
      const CRLSet* crl_set,
      CTVerifier* ct_verifier,
      const CTPolicyEnforcer* ct_policy_enforcer,
      CertNetFetcher* net_fetcher,
      VerificationType verification_type,
      bssl::SimplePathBuilderDelegate::DigestPolicy digest_policy,
      int flags,
      const CertVerifyProcTrustStore* trust_store,
      const std::vector<net::CertVerifyProc::CertificateWithConstraints>&
          additional_constraints,
      std::string_view stapled_leaf_ocsp_response,
      std::string_view sct_list_from_tls_extension,
      const EVRootCAMetadata* ev_metadata,
      base::TimeTicks deadline,
      base::Time current_time,
      bool* checked_revocation_for_some_path,
      const NetLogWithSource& net_log)
      : bssl::SimplePathBuilderDelegate(1024, digest_policy),
        crl_set_(crl_set),
        ct_verifier_(ct_verifier),
        ct_policy_enforcer_(ct_policy_enforcer),
        net_fetcher_(net_fetcher),
        verification_type_(verification_type),
        flags_(flags),
        trust_store_(trust_store),
        additional_constraints_(additional_constraints),
        stapled_leaf_ocsp_response_(stapled_leaf_ocsp_response),
        sct_list_from_tls_extension_(sct_list_from_tls_extension),
        ev_metadata_(ev_metadata),
        deadline_(deadline),
        current_time_(current_time),
        checked_revocation_for_some_path_(checked_revocation_for_some_path),
        net_log_(net_log) {}

  // This is called for each built chain, including ones which failed. It is
  // responsible for adding errors to the built chain if it is not acceptable.
  void CheckPathAfterVerification(
      const bssl::CertPathBuilder& path_builder,
      bssl::CertPathBuilderResultPath* path) override {
    net_log_->BeginEvent(NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT);

    CheckPathAfterVerificationImpl(path_builder, path);

    net_log_->EndEvent(NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                       [&] { return NetLogPathBuilderResultPath(*path); });
  }

 private:
  void CheckPathAfterVerificationImpl(const bssl::CertPathBuilder& path_builder,
                                      bssl::CertPathBuilderResultPath* path) {
    PathBuilderDelegateDataImpl* delegate_data =
        PathBuilderDelegateDataImpl::GetOrCreate(path);

    // TODO(https://crbug.com/1211074, https://crbug.com/848277): making a
    // temporary X509Certificate just to pass into CTVerifier and
    // CTPolicyEnforcer is silly, refactor so they take CRYPTO_BUFFER or
    // ParsedCertificate or something.
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    if (path->certs.size() > 1) {
      intermediates.push_back(bssl::UpRef(path->certs[1]->cert_buffer()));
    }
    auto cert_for_ct_verify = X509Certificate::CreateFromBuffer(
        bssl::UpRef(path->certs[0]->cert_buffer()), std::move(intermediates));
    ct_verifier_->Verify(cert_for_ct_verify.get(), stapled_leaf_ocsp_response_,
                         sct_list_from_tls_extension_, current_time_,
                         &delegate_data->scts, *net_log_);

    // Check any extra constraints that might exist outside of the certificates.
    CheckExtraConstraints(path->certs, &path->errors);
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
    CheckChromeRootConstraints(path);
#endif

    // If the path is already invalid, don't check revocation status. The
    // chain is expected to be valid when doing revocation checks (since for
    // instance the correct issuer for a certificate may need to be known).
    // Also if certificates are already expired, obtaining their revocation
    // status may fail.
    //
    // TODO(eroman): When CertVerifyProcBuiltin fails to find a valid path,
    //               whatever (partial/incomplete) path it does return should
    //               minimally be checked with the CRLSet.
    if (!path->IsValid()) {
      return;
    }

    // If EV was requested the certificate must chain to a recognized EV root
    // and have one of its recognized EV policy OIDs.
    if (verification_type_ == VerificationType::kEV) {
      if (!ConformsToEVPolicy(path)) {
        path->errors.GetErrorsForCert(0)->AddError(kPathLacksEVPolicy);
        return;
      }
    }

    // Select an appropriate revocation policy for this chain based on the
    // verifier flags and root.
    RevocationPolicy policy = ChooseRevocationPolicy(path->certs);

    // Check for revocations using the CRLSet.
    switch (
        CheckChainRevocationUsingCRLSet(crl_set_, path->certs, &path->errors)) {
      case CRLSet::Result::REVOKED:
        return;
      case CRLSet::Result::GOOD:
        break;
      case CRLSet::Result::UNKNOWN:
        // CRLSet was inconclusive.
        break;
    }

    if (policy.check_revocation) {
      *checked_revocation_for_some_path_ = true;
    }

    // Check the revocation status for each certificate in the chain according
    // to |policy|. Depending on the policy, errors will be added to the
    // respective certificates, so |errors->ContainsHighSeverityErrors()| will
    // reflect the revocation status of the chain after this call.
    CheckValidatedChainRevocation(path->certs, policy, deadline_,
                                  stapled_leaf_ocsp_response_, current_time_,
                                  net_fetcher_, &path->errors,
                                  &delegate_data->stapled_ocsp_verify_result);

    ct::SCTList verified_scts;
    for (const auto& sct_and_status : delegate_data->scts) {
      if (sct_and_status.status == ct::SCT_STATUS_OK) {
        verified_scts.push_back(sct_and_status.sct);
      }
    }
    delegate_data->ct_policy_compliance = ct_policy_enforcer_->CheckCompliance(
        cert_for_ct_verify.get(), verified_scts, current_time_, *net_log_);
  }

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // Returns the SCTs from `scts` that are verified successfully and signed by
  // a log which was not disqualified.
  ct::SCTList ValidScts(const SignedCertificateTimestampAndStatusList& scts) {
    ct::SCTList valid_scts;
    for (const auto& sct_and_status : scts) {
      if (sct_and_status.status != ct::SCT_STATUS_OK) {
        continue;
      }
      std::optional<base::Time> disqualification_time =
          ct_policy_enforcer_->GetLogDisqualificationTime(
              sct_and_status.sct->log_id);
      // TODO(https://crbug.com/40840044): use the same time source here as for
      // the rest of verification.
      if (disqualification_time && base::Time::Now() >= disqualification_time) {
        continue;
      }
      valid_scts.push_back(sct_and_status.sct);
    }
    return valid_scts;
  }

  bool CheckPathSatisfiesChromeRootConstraint(
      bssl::CertPathBuilderResultPath* path,
      const ChromeRootCertConstraints& constraint) {
    PathBuilderDelegateDataImpl* delegate_data =
        PathBuilderDelegateDataImpl::GetOrCreate(path);

    // TODO(https://crbug.com/40941039): add more specific netlog or CertError
    // logs about which constraint failed exactly? (Note that it could be
    // confusing when there are multiple ChromeRootCertConstraints objects,
    // would need to clearly distinguish which set of constraints had errors.)

    if (ct_policy_enforcer_->IsCtEnabled()) {
      if (constraint.sct_not_after.has_value()) {
        bool found_matching_sct = false;
        for (const auto& sct : ValidScts(delegate_data->scts)) {
          if (sct->timestamp <= constraint.sct_not_after.value()) {
            found_matching_sct = true;
            break;
          }
        }
        if (!found_matching_sct) {
          return false;
        }
      }

      if (constraint.sct_all_after.has_value()) {
        ct::SCTList valid_scts = ValidScts(delegate_data->scts);
        if (valid_scts.empty()) {
          return false;
        }
        for (const auto& sct : ValidScts(delegate_data->scts)) {
          if (sct->timestamp <= constraint.sct_all_after.value()) {
            return false;
          }
        }
      }
    }

    if (constraint.min_version.has_value() &&
        version_info::GetVersion() < constraint.min_version.value()) {
      return false;
    }

    if (constraint.max_version_exclusive.has_value() &&
        version_info::GetVersion() >=
            constraint.max_version_exclusive.value()) {
      return false;
    }

    return true;
  }

  void CheckChromeRootConstraints(bssl::CertPathBuilderResultPath* path) {
    // If the root is trusted locally, do not enforce CRS constraints, even if
    // some exist.
    if (trust_store_->IsNonChromeRootStoreTrustAnchor(
            path->certs.back().get())) {
      return;
    }

    if (base::span<const ChromeRootCertConstraints> constraints =
            trust_store_->GetChromeRootConstraints(path->certs.back().get());
        !constraints.empty()) {
      bool found_valid_constraint = false;
      for (const ChromeRootCertConstraints& constraint : constraints) {
        found_valid_constraint |=
            CheckPathSatisfiesChromeRootConstraint(path, constraint);
      }
      if (!found_valid_constraint) {
        path->errors.GetOtherErrors()->AddError(kChromeRootConstraintsFailed);
      }
    }
  }
#endif

  // Check extra constraints that aren't encoded in the certificates themselves.
  void CheckExtraConstraints(const bssl::ParsedCertificateList& certs,
                             bssl::CertPathErrors* errors) {
    const std::shared_ptr<const bssl::ParsedCertificate> root_cert =
        certs.back();
    // An assumption being made is that there will be at most a few (2-3) certs
    // in here; if there are more and this ends up being a drag on performance
    // it may be worth making additional_constraints_ into a map storing certs
    // by hash.
    for (const auto& cert_with_constraints : *additional_constraints_) {
      if (!x509_util::CryptoBufferEqual(
              root_cert->cert_buffer(),
              cert_with_constraints.certificate->cert_buffer())) {
        continue;
      }
      // Found the cert, check constraints
      if (cert_with_constraints.permitted_dns_names.empty() &&
          cert_with_constraints.permitted_cidrs.empty()) {
        // No constraints to check.
        return;
      }

      bssl::GeneralNames permitted_names;

      if (!cert_with_constraints.permitted_dns_names.empty()) {
        for (const auto& dns_name : cert_with_constraints.permitted_dns_names) {
          permitted_names.dns_names.push_back(dns_name);
        }
        permitted_names.present_name_types |=
            bssl::GeneralNameTypes::GENERAL_NAME_DNS_NAME;
      }

      if (!cert_with_constraints.permitted_cidrs.empty()) {
        for (const auto& cidr : cert_with_constraints.permitted_cidrs) {
          permitted_names.ip_address_ranges.emplace_back(cidr.ip.bytes(),
                                                         cidr.mask.bytes());
        }
        permitted_names.present_name_types |=
            bssl::GeneralNameTypes::GENERAL_NAME_IP_ADDRESS;
      }

      std::unique_ptr<bssl::NameConstraints> nc =
          bssl::NameConstraints::CreateFromPermittedSubtrees(
              std::move(permitted_names));

      const std::shared_ptr<const bssl::ParsedCertificate>& leaf_cert =
          certs[0];

      nc->IsPermittedCert(leaf_cert->normalized_subject(),
                          leaf_cert->subject_alt_names(),
                          errors->GetErrorsForCert(0));
      return;
    }
  }

  // Selects a revocation policy based on the CertVerifier flags and the given
  // certificate chain.
  RevocationPolicy ChooseRevocationPolicy(
      const bssl::ParsedCertificateList& certs) {
    if (flags_ & CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES) {
      // In theory when network fetches are disabled but revocation is enabled
      // we could continue with networking_allowed=false (and
      // VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS would also have to change
      // allow_missing_info and allow_unable_to_check to true).
      // That theoretically could allow still consulting any cached CRLs/etc.
      // However in the way things are currently implemented in the builtin
      // verifier there really is no point to bothering, just disable
      // revocation checking if network fetches are disabled.
      return NoRevocationChecking();
    }

    // Use hard-fail revocation checking for local trust anchors, if requested
    // by the load flag and the chain uses a non-public root.
    if ((flags_ & CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS) &&
        !certs.empty() && !trust_store_->IsKnownRoot(certs.back().get())) {
      RevocationPolicy policy;
      policy.check_revocation = true;
      policy.networking_allowed = true;
      policy.crl_allowed = true;
      policy.allow_missing_info = false;
      policy.allow_unable_to_check = false;
      policy.enforce_baseline_requirements = false;
      return policy;
    }

    // Use soft-fail revocation checking for VERIFY_REV_CHECKING_ENABLED.
    if (flags_ & CertVerifyProc::VERIFY_REV_CHECKING_ENABLED) {
      const bool is_known_root =
          !certs.empty() && trust_store_->IsKnownRoot(certs.back().get());
      RevocationPolicy policy;
      policy.check_revocation = true;
      policy.networking_allowed = true;
      // Publicly trusted certs are required to have OCSP by the Baseline
      // Requirements and CRLs can be quite large, so disable the fallback to
      // CRLs for chains to known roots.
      policy.crl_allowed = !is_known_root;
      policy.allow_missing_info = true;
      policy.allow_unable_to_check = true;
      policy.enforce_baseline_requirements = is_known_root;
      return policy;
    }

    return NoRevocationChecking();
  }

  // Returns true if |path| chains to an EV root, and the chain conforms to
  // one of its EV policy OIDs. When building paths all candidate EV policy
  // OIDs were requested, so it is just a matter of testing each of the
  // policies the chain conforms to.
  bool ConformsToEVPolicy(const bssl::CertPathBuilderResultPath* path) {
    const bssl::ParsedCertificate* root = path->GetTrustedCert();
    if (!root) {
      return false;
    }

    SHA256HashValue root_fingerprint;
    crypto::SHA256HashString(root->der_cert().AsStringView(),
                             root_fingerprint.data,
                             sizeof(root_fingerprint.data));

    for (const bssl::der::Input& oid : path->user_constrained_policy_set) {
      if (ev_metadata_->HasEVPolicyOID(root_fingerprint, oid)) {
        return true;
      }
    }

    return false;
  }

  bool IsDeadlineExpired() override {
    return !deadline_.is_null() && base::TimeTicks::Now() > deadline_;
  }

  bool IsDebugLogEnabled() override { return net_log_->IsCapturing(); }

  void DebugLog(std::string_view msg) override {
    net_log_->AddEventWithStringParams(
        NetLogEventType::CERT_VERIFY_PROC_PATH_BUILDER_DEBUG, "debug", msg);
  }

  raw_ptr<const CRLSet> crl_set_;
  raw_ptr<CTVerifier> ct_verifier_;
  raw_ptr<const CTPolicyEnforcer> ct_policy_enforcer_;
  raw_ptr<CertNetFetcher> net_fetcher_;
  const VerificationType verification_type_;
  const int flags_;
  raw_ptr<const CertVerifyProcTrustStore> trust_store_;
  raw_ref<const std::vector<net::CertVerifyProc::CertificateWithConstraints>>
      additional_constraints_;
  const std::string_view stapled_leaf_ocsp_response_;
  const std::string_view sct_list_from_tls_extension_;
  raw_ptr<const EVRootCAMetadata> ev_metadata_;
  base::TimeTicks deadline_;
  base::Time current_time_;
  raw_ptr<bool> checked_revocation_for_some_path_;
  raw_ref<const NetLogWithSource> net_log_;
};

std::shared_ptr<const bssl::ParsedCertificate> ParseCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    bssl::CertErrors* errors) {
  return bssl::ParsedCertificate::Create(
      bssl::UpRef(cert_handle), x509_util::DefaultParseCertificateOptions(),
      errors);
}

class CertVerifyProcBuiltin : public CertVerifyProc {
 public:
  CertVerifyProcBuiltin(scoped_refptr<CertNetFetcher> net_fetcher,
                        scoped_refptr<CRLSet> crl_set,
                        std::unique_ptr<CTVerifier> ct_verifier,
                        scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
                        std::unique_ptr<SystemTrustStore> system_trust_store,
                        const CertVerifyProc::InstanceParams& instance_params,
                        std::optional<network_time::TimeTracker> time_tracker);

 protected:
  ~CertVerifyProcBuiltin() override;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  const scoped_refptr<CertNetFetcher> net_fetcher_;
  const std::unique_ptr<CTVerifier> ct_verifier_;
  const scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer_;
  const std::unique_ptr<SystemTrustStore> system_trust_store_;
  std::vector<net::CertVerifyProc::CertificateWithConstraints>
      additional_constraints_;
  bssl::TrustStoreInMemory additional_trust_store_;
  const std::optional<network_time::TimeTracker> time_tracker_;
};

CertVerifyProcBuiltin::CertVerifyProcBuiltin(
    scoped_refptr<CertNetFetcher> net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<CTVerifier> ct_verifier,
    scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
    std::unique_ptr<SystemTrustStore> system_trust_store,
    const CertVerifyProc::InstanceParams& instance_params,
    std::optional<network_time::TimeTracker> time_tracker)
    : CertVerifyProc(std::move(crl_set)),
      net_fetcher_(std::move(net_fetcher)),
      ct_verifier_(std::move(ct_verifier)),
      ct_policy_enforcer_(std::move(ct_policy_enforcer)),
      system_trust_store_(std::move(system_trust_store)),
      time_tracker_(std::move(time_tracker)) {
  DCHECK(system_trust_store_);

  NetLogWithSource net_log =
      NetLogWithSource::Make(net::NetLogSourceType::CERT_VERIFY_PROC_CREATED);
  net_log.BeginEvent(NetLogEventType::CERT_VERIFY_PROC_CREATED);

  // When adding additional certs from instance params, there needs to be a
  // priority order if a cert is added with multiple different trust types.
  //
  // The priority is as follows:
  //
  //  (a) Distrusted SPKIs (though we don't check for SPKI collisions in added
  //      certs; we rely on that to happen in path building).
  //  (b) Trusted certs with enforced constraints both in the cert and
  //      specified externally outside of the cert.
  //  (c) Trusted certs with enforced constraints only within the cert.
  //  (d) Trusted certs w/o enforced constraints.
  //  (e) Unspecified certs.
  //
  //  No effort was made to categorize what applies if a cert is specified
  //  within the same category multiple times.

  for (const auto& spki : instance_params.additional_distrusted_spkis) {
    additional_trust_store_.AddDistrustedCertificateBySPKI(
        std::string(base::as_string_view(spki)));
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
      base::Value::Dict results;
      results.Set("spki", NetLogBinaryValue(base::make_span(spki)));
      results.Set("trust",
                  bssl::CertificateTrust::ForDistrusted().ToDebugString());
      return results;
    });
  }

  bssl::CertificateTrust anchor_trust_enforcement =
      bssl::CertificateTrust::ForTrustAnchor()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry();

  for (const auto& cert_with_constraints :
       instance_params.additional_trust_anchors_with_constraints) {
    const std::shared_ptr<const bssl::ParsedCertificate>& cert =
        cert_with_constraints.certificate;
    additional_trust_store_.AddCertificate(cert, anchor_trust_enforcement);
    additional_constraints_.push_back(cert_with_constraints);
    bssl::CertErrors parsing_errors;
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
      return NetLogAdditionalCert(cert->cert_buffer(),
                                  bssl::CertificateTrust::ForTrustAnchor(),
                                  parsing_errors);
    });
  }

  bssl::CertificateTrust leaf_trust = bssl::CertificateTrust::ForTrustedLeaf();

  for (const auto& cert_with_possible_constraints :
       instance_params.additional_trust_leafs) {
    const std::shared_ptr<const bssl::ParsedCertificate>& cert =
        cert_with_possible_constraints.certificate;
    if (!additional_trust_store_.Contains(cert.get())) {
      if (!cert_with_possible_constraints.permitted_dns_names.empty() ||
          !cert_with_possible_constraints.permitted_cidrs.empty()) {
        additional_constraints_.push_back(cert_with_possible_constraints);
      }

      bssl::CertErrors parsing_errors;
      additional_trust_store_.AddCertificate(cert, leaf_trust);
      net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
        return NetLogAdditionalCert(cert->cert_buffer(), leaf_trust,
                                    parsing_errors);
      });
    }
  }

  bssl::CertificateTrust anchor_leaf_trust =
      bssl::CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry();

  for (const auto& cert_with_possible_constraints :
       instance_params.additional_trust_anchors_and_leafs) {
    const std::shared_ptr<const bssl::ParsedCertificate>& cert =
        cert_with_possible_constraints.certificate;
    if (!additional_trust_store_.Contains(cert.get())) {
      if (!cert_with_possible_constraints.permitted_dns_names.empty() ||
          !cert_with_possible_constraints.permitted_cidrs.empty()) {
        additional_constraints_.push_back(cert_with_possible_constraints);
      }

      bssl::CertErrors parsing_errors;
      additional_trust_store_.AddCertificate(cert, anchor_leaf_trust);
      net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
        return NetLogAdditionalCert(cert->cert_buffer(), anchor_leaf_trust,
                                    parsing_errors);
      });
    }
  }

  for (const auto& cert :
       instance_params.additional_trust_anchors_with_enforced_constraints) {
    bssl::CertErrors parsing_errors;
    if (!additional_trust_store_.Contains(cert.get())) {
      additional_trust_store_.AddCertificate(cert, anchor_trust_enforcement);
      net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
        return NetLogAdditionalCert(cert->cert_buffer(),
                                    anchor_trust_enforcement, parsing_errors);
      });
    }
  }

  for (const auto& cert : instance_params.additional_trust_anchors) {
    bssl::CertErrors parsing_errors;
    // Only add if it wasn't already present in `additional_trust_store_`. This
    // is for two reasons:
    //   (1) TrustStoreInMemory doesn't expect to contain duplicates
    //   (2) If the same anchor is added with enforced constraints, that takes
    //       precedence.
    if (!additional_trust_store_.Contains(cert.get())) {
      additional_trust_store_.AddTrustAnchor(cert);
    }
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
      return NetLogAdditionalCert(cert->cert_buffer(),
                                  bssl::CertificateTrust::ForTrustAnchor(),
                                  parsing_errors);
    });
  }

  for (const auto& cert : instance_params.additional_untrusted_authorities) {
    bssl::CertErrors parsing_errors;
    // Only add the untrusted cert if it isn't already present in
    // `additional_trust_store_`. If the same cert was already added as a
    // trust anchor then adding it again as an untrusted cert can lead to it
    // not being treated as a trust anchor since TrustStoreInMemory doesn't
    // expect to contain duplicates.
    if (!additional_trust_store_.Contains(cert.get())) {
      additional_trust_store_.AddCertificateWithUnspecifiedTrust(cert);
    }
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_CERT, [&] {
      return NetLogAdditionalCert(cert->cert_buffer(),
                                  bssl::CertificateTrust::ForUnspecified(),
                                  parsing_errors);
    });
  }

  net_log.EndEvent(NetLogEventType::CERT_VERIFY_PROC_CREATED);
}

CertVerifyProcBuiltin::~CertVerifyProcBuiltin() = default;

void AddIntermediatesToIssuerSource(X509Certificate* x509_cert,
                                    bssl::CertIssuerSourceStatic* intermediates,
                                    const NetLogWithSource& net_log) {
  for (const auto& intermediate : x509_cert->intermediate_buffers()) {
    bssl::CertErrors errors;
    std::shared_ptr<const bssl::ParsedCertificate> cert =
        ParseCertificateFromBuffer(intermediate.get(), &errors);
    // TODO(crbug.com/40479281): this duplicates the logging of the input chain
    // maybe should only log if there is a parse error/warning?
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_INPUT_CERT, [&] {
      return NetLogCertParams(intermediate.get(), errors);
    });
    if (cert) {
      intermediates->AddCert(std::move(cert));
    }
  }
}

// Appends the SHA256 hashes of |spki_bytes| to |*hashes|.
// TODO(eroman): Hashes are also calculated at other times (such as when
//               checking CRLSet). Consider caching to avoid recalculating (say
//               in the delegate's PathInfo).
void AppendPublicKeyHashes(const bssl::der::Input& spki_bytes,
                           HashValueVector* hashes) {
  HashValue sha256(HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes.AsStringView(), sha256.data(),
                           crypto::kSHA256Length);
  hashes->push_back(sha256);
}

// Appends the SubjectPublicKeyInfo hashes for all certificates in
// |path| to |*hashes|.
void AppendPublicKeyHashes(const bssl::CertPathBuilderResultPath& path,
                           HashValueVector* hashes) {
  for (const std::shared_ptr<const bssl::ParsedCertificate>& cert :
       path.certs) {
    AppendPublicKeyHashes(cert->tbs().spki_tlv, hashes);
  }
}

// Sets the bits on |cert_status| for all the errors present in |errors| (the
// errors for a particular path).
void MapPathBuilderErrorsToCertStatus(const bssl::CertPathErrors& errors,
                                      CertStatus* cert_status) {
  // If there were no errors, nothing to do.
  if (!errors.ContainsHighSeverityErrors())
    return;

  if (errors.ContainsError(bssl::cert_errors::kCertificateRevoked)) {
    *cert_status |= CERT_STATUS_REVOKED;
  }

  if (errors.ContainsError(bssl::cert_errors::kNoRevocationMechanism)) {
    *cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;
  }

  if (errors.ContainsError(bssl::cert_errors::kUnableToCheckRevocation)) {
    *cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  }

  if (errors.ContainsError(bssl::cert_errors::kUnacceptablePublicKey)) {
    *cert_status |= CERT_STATUS_WEAK_KEY;
  }

  if (errors.ContainsError(bssl::cert_errors::kValidityFailedNotAfter) ||
      errors.ContainsError(bssl::cert_errors::kValidityFailedNotBefore)) {
    *cert_status |= CERT_STATUS_DATE_INVALID;
  }

  if (errors.ContainsError(bssl::cert_errors::kDistrustedByTrustStore) ||
      errors.ContainsError(bssl::cert_errors::kVerifySignedDataFailed) ||
      errors.ContainsError(bssl::cert_errors::kNoIssuersFound) ||
      errors.ContainsError(bssl::cert_errors::kSubjectDoesNotMatchIssuer) ||
      errors.ContainsError(bssl::cert_errors::kDeadlineExceeded) ||
      errors.ContainsError(bssl::cert_errors::kIterationLimitExceeded) ||
      errors.ContainsError(kChromeRootConstraintsFailed)) {
    *cert_status |= CERT_STATUS_AUTHORITY_INVALID;
  }

  // IMPORTANT: If the path was invalid for a reason that was not
  // explicity checked above, set a general error. This is important as
  // |cert_status| is what ultimately indicates whether verification was
  // successful or not (absence of errors implies success).
  if (!IsCertStatusError(*cert_status))
    *cert_status |= CERT_STATUS_INVALID;
}

// Creates a X509Certificate (chain) to return as the verified result.
//
//  * |target_cert|: The original X509Certificate that was passed in to
//                   VerifyInternal()
//  * |path|: The result (possibly failed) from path building.
scoped_refptr<X509Certificate> CreateVerifiedCertChain(
    X509Certificate* target_cert,
    const bssl::CertPathBuilderResultPath& path) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;

  // Skip the first certificate in the path as that is the target certificate
  for (size_t i = 1; i < path.certs.size(); ++i) {
    intermediates.push_back(bssl::UpRef(path.certs[i]->cert_buffer()));
  }

  scoped_refptr<X509Certificate> result =
      target_cert->CloneWithDifferentIntermediates(std::move(intermediates));
  DCHECK(result);

  return result;
}

// Describes the parameters for a single path building attempt. Path building
// may be re-tried with different parameters for EV and for accepting SHA1
// certificates.
struct BuildPathAttempt {
  BuildPathAttempt(VerificationType verification_type,
                   bssl::SimplePathBuilderDelegate::DigestPolicy digest_policy,
                   bool use_system_time)
      : verification_type(verification_type),
        digest_policy(digest_policy),
        use_system_time(use_system_time) {}

  BuildPathAttempt(VerificationType verification_type, bool use_system_time)
      : BuildPathAttempt(verification_type,
                         bssl::SimplePathBuilderDelegate::DigestPolicy::kStrong,
                         use_system_time) {}

  VerificationType verification_type;
  bssl::SimplePathBuilderDelegate::DigestPolicy digest_policy;
  bool use_system_time;
};

bssl::CertPathBuilder::Result TryBuildPath(
    const std::shared_ptr<const bssl::ParsedCertificate>& target,
    bssl::CertIssuerSourceStatic* intermediates,
    CertVerifyProcTrustStore* trust_store,
    const std::vector<net::CertVerifyProc::CertificateWithConstraints>&
        additional_constraints,
    const bssl::der::GeneralizedTime& der_verification_time,
    base::Time current_time,
    base::TimeTicks deadline,
    VerificationType verification_type,
    bssl::SimplePathBuilderDelegate::DigestPolicy digest_policy,
    int flags,
    std::string_view ocsp_response,
    std::string_view sct_list,
    const CRLSet* crl_set,
    CTVerifier* ct_verifier,
    const CTPolicyEnforcer* ct_policy_enforcer,
    CertNetFetcher* net_fetcher,
    const EVRootCAMetadata* ev_metadata,
    bool* checked_revocation,
    const NetLogWithSource& net_log) {
  // Path building will require candidate paths to conform to at least one of
  // the policies in |user_initial_policy_set|.
  std::set<bssl::der::Input> user_initial_policy_set;

  if (verification_type == VerificationType::kEV) {
    GetEVPolicyOids(ev_metadata, target.get(), &user_initial_policy_set);
    // TODO(crbug.com/40479281): netlog user_initial_policy_set.
  } else {
    user_initial_policy_set = {bssl::der::Input(bssl::kAnyPolicyOid)};
  }

  PathBuilderDelegateImpl path_builder_delegate(
      crl_set, ct_verifier, ct_policy_enforcer, net_fetcher, verification_type,
      digest_policy, flags, trust_store, additional_constraints, ocsp_response,
      sct_list, ev_metadata, deadline, current_time, checked_revocation,
      net_log);

  std::optional<CertIssuerSourceAia> aia_cert_issuer_source;

  // Initialize the path builder.
  bssl::CertPathBuilder path_builder(
      target, trust_store->trust_store(), &path_builder_delegate,
      der_verification_time, bssl::KeyPurpose::SERVER_AUTH,
      bssl::InitialExplicitPolicy::kFalse, user_initial_policy_set,
      bssl::InitialPolicyMappingInhibit::kFalse,
      bssl::InitialAnyPolicyInhibit::kFalse);

  // Allow the path builder to discover the explicitly provided intermediates in
  // |input_cert|.
  path_builder.AddCertIssuerSource(intermediates);

  // Allow the path builder to discover intermediates through AIA fetching.
  // TODO(crbug.com/40479281): hook up netlog to AIA.
  if (!(flags & CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES)) {
    if (net_fetcher) {
      aia_cert_issuer_source.emplace(net_fetcher);
      path_builder.AddCertIssuerSource(&aia_cert_issuer_source.value());
    } else {
      VLOG(1) << "No net_fetcher for performing AIA chasing.";
    }
  }

  path_builder.SetIterationLimit(kPathBuilderIterationLimit);

  return path_builder.Run();
}

int AssignVerifyResult(X509Certificate* input_cert,
                       const std::string& hostname,
                       bssl::CertPathBuilder::Result& result,
                       VerificationType verification_type,
                       bool checked_revocation_for_some_path,
                       CertVerifyProcTrustStore* trust_store,
                       CertVerifyResult* verify_result) {
  const bssl::CertPathBuilderResultPath* best_path_possibly_invalid =
      result.GetBestPathPossiblyInvalid();

  if (!best_path_possibly_invalid) {
    // TODO(crbug.com/41267838): What errors to communicate? Maybe the path
    // builder should always return some partial path (even if just containing
    // the target), then there is a bssl::CertErrors to test.
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    return ERR_CERT_AUTHORITY_INVALID;
  }

  const bssl::CertPathBuilderResultPath& partial_path =
      *best_path_possibly_invalid;

  AppendPublicKeyHashes(partial_path, &verify_result->public_key_hashes);

  bool path_is_valid = partial_path.IsValid();

  const bssl::ParsedCertificate* trusted_cert = partial_path.GetTrustedCert();
  if (trusted_cert) {
    verify_result->is_issued_by_known_root =
        trust_store->IsKnownRoot(trusted_cert);

    verify_result->is_issued_by_additional_trust_anchor =
        trust_store->IsAdditionalTrustAnchor(trusted_cert);
  }

  if (path_is_valid && (verification_type == VerificationType::kEV)) {
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  }

  // TODO(eroman): Add documentation for the meaning of
  // CERT_STATUS_REV_CHECKING_ENABLED. Based on the current tests it appears to
  // mean whether revocation checking was attempted during path building,
  // although does not necessarily mean that revocation checking was done for
  // the final returned path.
  if (checked_revocation_for_some_path)
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

  verify_result->verified_cert =
      CreateVerifiedCertChain(input_cert, partial_path);

  MapPathBuilderErrorsToCertStatus(partial_path.errors,
                                   &verify_result->cert_status);

  // TODO(eroman): Is it possible that IsValid() fails but no errors were set in
  // partial_path.errors?
  CHECK(path_is_valid || IsCertStatusError(verify_result->cert_status));

  if (!path_is_valid) {
    VLOG(1) << "CertVerifyProcBuiltin for " << hostname << " failed:\n"
            << partial_path.errors.ToDebugString(partial_path.certs);
  }

  const PathBuilderDelegateDataImpl* delegate_data =
      PathBuilderDelegateDataImpl::Get(partial_path);
  if (delegate_data) {
    verify_result->ocsp_result = delegate_data->stapled_ocsp_verify_result;
    verify_result->scts = std::move(delegate_data->scts);
    verify_result->policy_compliance = delegate_data->ct_policy_compliance;
  }

  return IsCertStatusError(verify_result->cert_status)
             ? MapCertStatusToNetError(verify_result->cert_status)
             : OK;
}

// Returns true if retrying path building with a less stringent signature
// algorithm *might* successfully build a path, based on the earlier failed
// |result|.
//
// This implementation is simplistic, and looks only for the presence of the
// kUnacceptableSignatureAlgorithm error somewhere among the built paths.
bool CanTryAgainWithWeakerDigestPolicy(
    const bssl::CertPathBuilder::Result& result) {
  return result.AnyPathContainsError(
      bssl::cert_errors::kUnacceptableSignatureAlgorithm);
}

// Returns true if retrying with the system time as the verification time might
// successfully build a path, based on the earlier failed |result|.
bool CanTryAgainWithSystemTime(const bssl::CertPathBuilder::Result& result) {
  // TODO(crbug.com/363034686): Retries should also be triggered for CT
  // failures.
  return result.AnyPathContainsError(
             bssl::cert_errors::kValidityFailedNotAfter) ||
         result.AnyPathContainsError(
             bssl::cert_errors::kValidityFailedNotBefore) ||
         result.AnyPathContainsError(bssl::cert_errors::kCertificateRevoked) ||
         result.AnyPathContainsError(
             bssl::cert_errors::kUnableToCheckRevocation);
}

int CertVerifyProcBuiltin::VerifyInternal(X509Certificate* input_cert,
                                          const std::string& hostname,
                                          const std::string& ocsp_response,
                                          const std::string& sct_list,
                                          int flags,
                                          CertVerifyResult* verify_result,
                                          const NetLogWithSource& net_log) {
  base::TimeTicks deadline = base::TimeTicks::Now() + kMaxVerificationTime;
  bssl::der::GeneralizedTime der_verification_system_time;
  bssl::der::GeneralizedTime der_verification_custom_time;
  if (!EncodeTimeAsGeneralizedTime(base::Time::Now(),
                                   &der_verification_system_time)) {
    // This shouldn't be possible.
    // We don't really have a good error code for this type of error.
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    return ERR_CERT_AUTHORITY_INVALID;
  }
  bool custom_time_available = false;
  base::Time custom_time;
  if (time_tracker_.has_value()) {
    custom_time_available = time_tracker_->GetTime(
        base::Time::Now(), base::TimeTicks::Now(), &custom_time, nullptr);
    if (custom_time_available &&
        !EncodeTimeAsGeneralizedTime(custom_time,
                                     &der_verification_custom_time)) {
      // This shouldn't be possible, but if it somehow happens, just use system
      // time.
      custom_time_available = false;
    }
  }
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  int64_t chrome_root_store_version =
      system_trust_store_->chrome_root_store_version();
  if (chrome_root_store_version != 0) {
    net_log.AddEvent(
        NetLogEventType::CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION, [&] {
          return NetLogChromeRootStoreVersion(chrome_root_store_version);
        });
  }
#endif

  // TODO(crbug.com/40928765): Netlog extra configuration information stored
  // inside CertVerifyProcBuiltin (e.g. certs in additional_trust_store and
  // system trust store)

  // Parse the target certificate.
  std::shared_ptr<const bssl::ParsedCertificate> target;
  {
    bssl::CertErrors parsing_errors;
    target =
        ParseCertificateFromBuffer(input_cert->cert_buffer(), &parsing_errors);
    // TODO(crbug.com/40479281): this duplicates the logging of the input chain
    // maybe should only log if there is a parse error/warning?
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_TARGET_CERT, [&] {
      return NetLogCertParams(input_cert->cert_buffer(), parsing_errors);
    });
    if (!target) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return ERR_CERT_INVALID;
    }
  }

  // Parse the provided intermediates.
  bssl::CertIssuerSourceStatic intermediates;
  AddIntermediatesToIssuerSource(input_cert, &intermediates, net_log);

  CertVerifyProcTrustStore trust_store(system_trust_store_.get(),
                                       &additional_trust_store_);

  // Get the global dependencies.
  const EVRootCAMetadata* ev_metadata = EVRootCAMetadata::GetInstance();

  // This boolean tracks whether online revocation checking was performed for
  // *any* of the built paths, and not just the final path returned (used for
  // setting output flag CERT_STATUS_REV_CHECKING_ENABLED).
  bool checked_revocation_for_some_path = false;

  // Run path building with the different parameters (attempts) until a valid
  // path is found. Earlier successful attempts have priority over later
  // attempts.
  //
  // Attempts are enqueued into |attempts| and drained in FIFO order.
  std::vector<BuildPathAttempt> attempts;

  // First try EV validation. Can skip this if the leaf certificate has no
  // chance of verifying as EV (lacks an EV policy).
  if (IsEVCandidate(ev_metadata, target.get()))
    attempts.emplace_back(VerificationType::kEV, !custom_time_available);

  // Next try DV validation.
  attempts.emplace_back(VerificationType::kDV, !custom_time_available);

  bssl::CertPathBuilder::Result result;
  VerificationType verification_type = VerificationType::kDV;

  // Iterate over |attempts| until there are none left to try, or an attempt
  // succeeded.
  for (size_t cur_attempt_index = 0; cur_attempt_index < attempts.size();
       ++cur_attempt_index) {
    const auto& cur_attempt = attempts[cur_attempt_index];
    verification_type = cur_attempt.verification_type;
    net_log.BeginEvent(
        NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT, [&] {
          base::Value::Dict results;
          if (verification_type == VerificationType::kEV)
            results.Set("is_ev_attempt", true);
          results.Set("is_network_time_attempt", !cur_attempt.use_system_time);
          if (!cur_attempt.use_system_time) {
            results.Set(
                "network_time_value",
                NetLogNumberValue(custom_time.InMillisecondsSinceUnixEpoch()));
          }
          results.Set("digest_policy",
                      static_cast<int>(cur_attempt.digest_policy));
          return results;
        });

    // If a previous attempt used up most/all of the deadline, extend the
    // deadline a little bit to give this verification attempt a chance at
    // success.
    deadline = std::max(
        deadline, base::TimeTicks::Now() + kPerAttemptMinVerificationTimeLimit);

    // Run the attempt through the path builder.
    result = TryBuildPath(
        target, &intermediates, &trust_store, additional_constraints_,
        cur_attempt.use_system_time ? der_verification_system_time
                                    : der_verification_custom_time,
        cur_attempt.use_system_time ? base::Time::Now() : custom_time, deadline,
        cur_attempt.verification_type, cur_attempt.digest_policy, flags,
        ocsp_response, sct_list, crl_set(), ct_verifier_.get(),
        ct_policy_enforcer_.get(), net_fetcher_.get(), ev_metadata,
        &checked_revocation_for_some_path, net_log);

    net_log.EndEvent(NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
                     [&] { return NetLogPathBuilderResult(result); });

    if (result.HasValidPath())
      break;

    if (result.exceeded_deadline) {
      // Stop immediately if an attempt exceeds the deadline.
      break;
    }

    if (!cur_attempt.use_system_time && CanTryAgainWithSystemTime(result)) {
      BuildPathAttempt system_time_attempt = cur_attempt;
      system_time_attempt.use_system_time = true;
      attempts.push_back(system_time_attempt);
    } else if (cur_attempt.digest_policy ==
                   bssl::SimplePathBuilderDelegate::DigestPolicy::kStrong &&
               CanTryAgainWithWeakerDigestPolicy(result)) {
      // If this path building attempt (may have) failed due to the chain using
      // a
      // weak signature algorithm, enqueue a similar attempt but with weaker
      // signature algorithms (SHA1) permitted.
      //
      // This fallback is necessary because the CertVerifyProc layer may decide
      // to allow SHA1 based on its own policy, so path building should return
      // possibly weak chains too.
      //
      // TODO(eroman): Would be better for the SHA1 policy to be part of the
      // delegate instead so it can interact with path building.
      BuildPathAttempt sha1_fallback_attempt = cur_attempt;
      sha1_fallback_attempt.digest_policy =
          bssl::SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1;
      attempts.push_back(sha1_fallback_attempt);
    }
  }

  // Write the results to |*verify_result|.
  int error = AssignVerifyResult(
      input_cert, hostname, result, verification_type,
      checked_revocation_for_some_path, &trust_store, verify_result);
  if (error == OK) {
    LogNameNormalizationMetrics(".Builtin", verify_result->verified_cert.get(),
                                verify_result->is_issued_by_known_root);
  }
  return error;
}

}  // namespace

scoped_refptr<CertVerifyProc> CreateCertVerifyProcBuiltin(
    scoped_refptr<CertNetFetcher> net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<CTVerifier> ct_verifier,
    scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
    std::unique_ptr<SystemTrustStore> system_trust_store,
    const CertVerifyProc::InstanceParams& instance_params,
    std::optional<network_time::TimeTracker> time_tracker) {
  return base::MakeRefCounted<CertVerifyProcBuiltin>(
      std::move(net_fetcher), std::move(crl_set), std::move(ct_verifier),
      std::move(ct_policy_enforcer), std::move(system_trust_store),
      instance_params, std::move(time_tracker));
}

base::TimeDelta GetCertVerifyProcBuiltinTimeLimitForTesting() {
  return kMaxVerificationTime;
}

}  // namespace net
