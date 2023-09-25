// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_issuer_source_aia.h"
#include "net/cert/internal/revocation_checker.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/known_roots.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/common_cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/path_builder.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/pki/trust_store_collection.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

// Very conservative iteration count limit.
// TODO(https://crbug.com/634470): Make this smaller.
constexpr uint32_t kPathBuilderIterationLimit = 25000;

constexpr base::TimeDelta kMaxVerificationTime = base::Seconds(60);

constexpr base::TimeDelta kPerAttemptMinVerificationTimeLimit =
    base::Seconds(5);

DEFINE_CERT_ERROR_ID(kPathLacksEVPolicy, "Path does not have an EV policy");

base::Value::Dict NetLogCertParams(const CRYPTO_BUFFER* cert_handle,
                                   const CertErrors& errors) {
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

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
base::Value::Dict NetLogChromeRootStoreVersion(
    int64_t chrome_root_store_version) {
  base::Value::Dict results;
  results.Set("version_major", NetLogNumberValue(chrome_root_store_version));
  return results;
}
#endif

base::Value::List PEMCertValueList(const ParsedCertificateList& certs) {
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
    const CertPathBuilderResultPath& result_path) {
  base::Value::Dict dict;
  dict.Set("is_valid", result_path.IsValid());
  dict.Set("last_cert_trust", result_path.last_cert_trust.ToDebugString());
  dict.Set("certificates", PEMCertValueList(result_path.certs));
  // TODO(crbug.com/634484): netlog user_constrained_policy_set.
  std::string errors_string =
      result_path.errors.ToDebugString(result_path.certs);
  if (!errors_string.empty())
    dict.Set("errors", errors_string);
  return dict;
}

base::Value::Dict NetLogPathBuilderResult(
    const CertPathBuilder::Result& result) {
  base::Value::Dict dict;
  // TODO(crbug.com/634484): include debug data (or just have things netlog it
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
                     const ParsedCertificate* cert,
                     std::set<der::Input>* oids) {
  oids->clear();

  if (!cert->has_policy_oids())
    return;

  for (const der::Input& oid : cert->policy_oids()) {
    if (ev_metadata->IsEVPolicyOID(oid)) {
      oids->insert(oid);
    }
  }
}

// Returns true if |cert| could be an EV certificate, based on its policies
// extension. A return of false means it definitely is not an EV certificate,
// whereas a return of true means it could be EV.
bool IsEVCandidate(const EVRootCAMetadata* ev_metadata,
                   const ParsedCertificate* cert) {
  std::set<der::Input> oids;
  GetEVPolicyOids(ev_metadata, cert, &oids);
  return !oids.empty();
}

// CertVerifyProcTrustStore wraps a SystemTrustStore with additional trust
// anchors and TestRootCerts.
class CertVerifyProcTrustStore {
 public:
  // |system_trust_store| must outlive this object.
  explicit CertVerifyProcTrustStore(SystemTrustStore* system_trust_store)
      : system_trust_store_(system_trust_store) {
    trust_store_.AddTrustStore(&additional_trust_store_);
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

  TrustStore* trust_store() { return &trust_store_; }

  void AddTrustAnchor(std::shared_ptr<const ParsedCertificate> cert) {
    additional_trust_store_.AddTrustAnchor(std::move(cert));
  }

  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const {
    if (TestRootCerts::HasInstance() &&
        TestRootCerts::GetInstance()->IsKnownRoot(
            trust_anchor->der_cert().AsSpan())) {
      return true;
    }
    return system_trust_store_->IsKnownRoot(trust_anchor);
  }

  bool IsAdditionalTrustAnchor(const ParsedCertificate* trust_anchor) const {
    return additional_trust_store_.Contains(trust_anchor);
  }

 private:
  raw_ptr<SystemTrustStore> system_trust_store_;
  TrustStoreInMemory additional_trust_store_;
  TrustStoreCollection trust_store_;
};

// Enum for whether path building is attempting to verify a certificate as EV or
// as DV.
enum class VerificationType {
  kEV,  // Extended Validation
  kDV,  // Domain Validation
};

class PathBuilderDelegateDataImpl : public CertPathBuilderDelegateData {
 public:
  ~PathBuilderDelegateDataImpl() override = default;

  static const PathBuilderDelegateDataImpl* Get(
      const CertPathBuilderResultPath& path) {
    return static_cast<PathBuilderDelegateDataImpl*>(path.delegate_data.get());
  }

  static PathBuilderDelegateDataImpl* GetOrCreate(
      CertPathBuilderResultPath* path) {
    if (!path->delegate_data)
      path->delegate_data = std::make_unique<PathBuilderDelegateDataImpl>();
    return static_cast<PathBuilderDelegateDataImpl*>(path->delegate_data.get());
  }

  OCSPVerifyResult stapled_ocsp_verify_result;
};

// TODO(eroman): The path building code in this file enforces its idea of weak
// keys, and signature algorithms, but separately cert_verify_proc.cc also
// checks the chains with its own policy. These policies must be aligned to
// give path building the best chance of finding a good path.
class PathBuilderDelegateImpl : public SimplePathBuilderDelegate {
 public:
  // Uses the default policy from SimplePathBuilderDelegate, which requires RSA
  // keys to be at least 1024-bits large, and optionally accepts SHA1
  // certificates.
  PathBuilderDelegateImpl(const CRLSet* crl_set,
                          CertNetFetcher* net_fetcher,
                          VerificationType verification_type,
                          SimplePathBuilderDelegate::DigestPolicy digest_policy,
                          int flags,
                          const CertVerifyProcTrustStore* trust_store,
                          base::StringPiece stapled_leaf_ocsp_response,
                          const EVRootCAMetadata* ev_metadata,
                          bool* checked_revocation_for_some_path,
                          base::TimeTicks deadline)
      : SimplePathBuilderDelegate(1024, digest_policy),
        crl_set_(crl_set),
        net_fetcher_(net_fetcher),
        verification_type_(verification_type),
        flags_(flags),
        trust_store_(trust_store),
        stapled_leaf_ocsp_response_(stapled_leaf_ocsp_response),
        ev_metadata_(ev_metadata),
        checked_revocation_for_some_path_(checked_revocation_for_some_path),
        deadline_(deadline) {}

  // This is called for each built chain, including ones which failed. It is
  // responsible for adding errors to the built chain if it is not acceptable.
  void CheckPathAfterVerification(const CertPathBuilder& path_builder,
                                  CertPathBuilderResultPath* path) override {
    // If the path is already invalid, don't check revocation status. The chain
    // is expected to be valid when doing revocation checks (since for instance
    // the correct issuer for a certificate may need to be known). Also if
    // certificates are already expired, obtaining their revocation status may
    // fail.
    //
    // TODO(eroman): When CertVerifyProcBuiltin fails to find a valid path,
    //               whatever (partial/incomplete) path it does return should
    //               minimally be checked with the CRLSet.
    if (!path->IsValid())
      return;

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

    if (policy.check_revocation)
      *checked_revocation_for_some_path_ = true;

    // Check the revocation status for each certificate in the chain according
    // to |policy|. Depending on the policy, errors will be added to the
    // respective certificates, so |errors->ContainsHighSeverityErrors()| will
    // reflect the revocation status of the chain after this call.
    CheckValidatedChainRevocation(
        path->certs, policy, deadline_, stapled_leaf_ocsp_response_,
        net_fetcher_, &path->errors,
        &PathBuilderDelegateDataImpl::GetOrCreate(path)
             ->stapled_ocsp_verify_result);
  }

 private:
  // Selects a revocation policy based on the CertVerifier flags and the given
  // certificate chain.
  RevocationPolicy ChooseRevocationPolicy(const ParsedCertificateList& certs) {
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

  // Returns true if |path| chains to an EV root, and the chain conforms to one
  // of its EV policy OIDs. When building paths all candidate EV policy OIDs
  // were requested, so it is just a matter of testing each of the policies the
  // chain conforms to.
  bool ConformsToEVPolicy(const CertPathBuilderResultPath* path) {
    const ParsedCertificate* root = path->GetTrustedCert();
    if (!root)
      return false;

    SHA256HashValue root_fingerprint;
    crypto::SHA256HashString(root->der_cert().AsStringView(),
                             root_fingerprint.data,
                             sizeof(root_fingerprint.data));

    for (const der::Input& oid : path->user_constrained_policy_set) {
      if (ev_metadata_->HasEVPolicyOID(root_fingerprint, oid)) {
        return true;
      }
    }

    return false;
  }

  bool IsDeadlineExpired() override {
    return !deadline_.is_null() && base::TimeTicks::Now() > deadline_;
  }

  raw_ptr<const CRLSet> crl_set_;
  raw_ptr<CertNetFetcher> net_fetcher_;
  const VerificationType verification_type_;
  const int flags_;
  raw_ptr<const CertVerifyProcTrustStore> trust_store_;
  const base::StringPiece stapled_leaf_ocsp_response_;
  raw_ptr<const EVRootCAMetadata> ev_metadata_;
  raw_ptr<bool> checked_revocation_for_some_path_;
  base::TimeTicks deadline_;
};

class CertVerifyProcBuiltin : public CertVerifyProc {
 public:
  CertVerifyProcBuiltin(scoped_refptr<CertNetFetcher> net_fetcher,
                        scoped_refptr<CRLSet> crl_set,
                        std::unique_ptr<SystemTrustStore> system_trust_store);

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcBuiltin() override;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  const scoped_refptr<CertNetFetcher> net_fetcher_;
  const std::unique_ptr<SystemTrustStore> system_trust_store_;
};

CertVerifyProcBuiltin::CertVerifyProcBuiltin(
    scoped_refptr<CertNetFetcher> net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<SystemTrustStore> system_trust_store)
    : CertVerifyProc(std::move(crl_set)),
      net_fetcher_(std::move(net_fetcher)),
      system_trust_store_(std::move(system_trust_store)) {
  DCHECK(system_trust_store_);
}

CertVerifyProcBuiltin::~CertVerifyProcBuiltin() = default;

bool CertVerifyProcBuiltin::SupportsAdditionalTrustAnchors() const {
  return true;
}

std::shared_ptr<const ParsedCertificate> ParseCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    CertErrors* errors) {
  return ParsedCertificate::Create(bssl::UpRef(cert_handle),
                                   x509_util::DefaultParseCertificateOptions(),
                                   errors);
}

void AddIntermediatesToIssuerSource(X509Certificate* x509_cert,
                                    CertIssuerSourceStatic* intermediates,
                                    const NetLogWithSource& net_log) {
  for (const auto& intermediate : x509_cert->intermediate_buffers()) {
    CertErrors errors;
    std::shared_ptr<const ParsedCertificate> cert =
        ParseCertificateFromBuffer(intermediate.get(), &errors);
    // TODO(crbug.com/634484): this duplicates the logging of the input chain
    // maybe should only log if there is a parse error/warning?
    net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_INPUT_CERT, [&] {
      return NetLogCertParams(intermediate.get(), errors);
    });
    if (cert)
      intermediates->AddCert(std::move(cert));
  }
}

// Appends the SHA256 hashes of |spki_bytes| to |*hashes|.
// TODO(eroman): Hashes are also calculated at other times (such as when
//               checking CRLSet). Consider caching to avoid recalculating (say
//               in the delegate's PathInfo).
void AppendPublicKeyHashes(const der::Input& spki_bytes,
                           HashValueVector* hashes) {
  HashValue sha256(HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes.AsStringView(), sha256.data(),
                           crypto::kSHA256Length);
  hashes->push_back(sha256);
}

// Appends the SubjectPublicKeyInfo hashes for all certificates in
// |path| to |*hashes|.
void AppendPublicKeyHashes(const CertPathBuilderResultPath& path,
                           HashValueVector* hashes) {
  for (const std::shared_ptr<const ParsedCertificate>& cert : path.certs)
    AppendPublicKeyHashes(cert->tbs().spki_tlv, hashes);
}

// Sets the bits on |cert_status| for all the errors present in |errors| (the
// errors for a particular path).
void MapPathBuilderErrorsToCertStatus(const CertPathErrors& errors,
                                      CertStatus* cert_status) {
  // If there were no errors, nothing to do.
  if (!errors.ContainsHighSeverityErrors())
    return;

  if (errors.ContainsError(cert_errors::kCertificateRevoked))
    *cert_status |= CERT_STATUS_REVOKED;

  if (errors.ContainsError(cert_errors::kNoRevocationMechanism))
    *cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (errors.ContainsError(cert_errors::kUnableToCheckRevocation))
    *cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (errors.ContainsError(cert_errors::kUnacceptablePublicKey))
    *cert_status |= CERT_STATUS_WEAK_KEY;

  if (errors.ContainsError(cert_errors::kValidityFailedNotAfter) ||
      errors.ContainsError(cert_errors::kValidityFailedNotBefore)) {
    *cert_status |= CERT_STATUS_DATE_INVALID;
  }

  if (errors.ContainsError(cert_errors::kDistrustedByTrustStore) ||
      errors.ContainsError(cert_errors::kVerifySignedDataFailed) ||
      errors.ContainsError(cert_errors::kNoIssuersFound) ||
      errors.ContainsError(cert_errors::kSubjectDoesNotMatchIssuer) ||
      errors.ContainsError(cert_errors::kDeadlineExceeded) ||
      errors.ContainsError(cert_errors::kIterationLimitExceeded)) {
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
    const CertPathBuilderResultPath& path) {
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
                   SimplePathBuilderDelegate::DigestPolicy digest_policy)
      : verification_type(verification_type), digest_policy(digest_policy) {}

  explicit BuildPathAttempt(VerificationType verification_type)
      : BuildPathAttempt(verification_type,
                         SimplePathBuilderDelegate::DigestPolicy::kStrong) {}

  VerificationType verification_type;
  SimplePathBuilderDelegate::DigestPolicy digest_policy;
};

CertPathBuilder::Result TryBuildPath(
    const std::shared_ptr<const ParsedCertificate>& target,
    CertIssuerSourceStatic* intermediates,
    CertVerifyProcTrustStore* trust_store,
    const der::GeneralizedTime& der_verification_time,
    base::TimeTicks deadline,
    VerificationType verification_type,
    SimplePathBuilderDelegate::DigestPolicy digest_policy,
    int flags,
    const std::string& ocsp_response,
    const CRLSet* crl_set,
    CertNetFetcher* net_fetcher,
    const EVRootCAMetadata* ev_metadata,
    bool* checked_revocation) {
  // Path building will require candidate paths to conform to at least one of
  // the policies in |user_initial_policy_set|.
  std::set<der::Input> user_initial_policy_set;

  if (verification_type == VerificationType::kEV) {
    GetEVPolicyOids(ev_metadata, target.get(), &user_initial_policy_set);
    // TODO(crbug.com/634484): netlog user_initial_policy_set.
  } else {
    user_initial_policy_set = {der::Input(kAnyPolicyOid)};
  }

  PathBuilderDelegateImpl path_builder_delegate(
      crl_set, net_fetcher, verification_type, digest_policy, flags,
      trust_store, ocsp_response, ev_metadata, checked_revocation, deadline);

  absl::optional<CertIssuerSourceAia> aia_cert_issuer_source;

  // Initialize the path builder.
  CertPathBuilder path_builder(
      target, trust_store->trust_store(), &path_builder_delegate,
      der_verification_time, KeyPurpose::SERVER_AUTH,
      InitialExplicitPolicy::kFalse, user_initial_policy_set,
      InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse);

  // Allow the path builder to discover the explicitly provided intermediates in
  // |input_cert|.
  path_builder.AddCertIssuerSource(intermediates);

  // Allow the path builder to discover intermediates through AIA fetching.
  // TODO(crbug.com/634484): hook up netlog to AIA.
  if (!(flags & CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES)) {
    if (net_fetcher) {
      aia_cert_issuer_source.emplace(net_fetcher);
      path_builder.AddCertIssuerSource(&aia_cert_issuer_source.value());
    } else {
      LOG(ERROR) << "No net_fetcher for performing AIA chasing.";
    }
  }

  path_builder.SetIterationLimit(kPathBuilderIterationLimit);

  return path_builder.Run();
}

int AssignVerifyResult(X509Certificate* input_cert,
                       const std::string& hostname,
                       CertPathBuilder::Result& result,
                       VerificationType verification_type,
                       bool checked_revocation_for_some_path,
                       CertVerifyProcTrustStore* trust_store,
                       CertVerifyResult* verify_result) {
  const CertPathBuilderResultPath* best_path_possibly_invalid =
      result.GetBestPathPossiblyInvalid();

  if (!best_path_possibly_invalid) {
    // TODO(crbug.com/634443): What errors to communicate? Maybe the path
    // builder should always return some partial path (even if just containing
    // the target), then there is a CertErrors to test.
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    return ERR_CERT_AUTHORITY_INVALID;
  }

  const CertPathBuilderResultPath& partial_path = *best_path_possibly_invalid;

  AppendPublicKeyHashes(partial_path, &verify_result->public_key_hashes);

  for (auto it = verify_result->public_key_hashes.rbegin();
       it != verify_result->public_key_hashes.rend() &&
       !verify_result->is_issued_by_known_root;
       ++it) {
    verify_result->is_issued_by_known_root =
        GetNetTrustAnchorHistogramIdForSPKI(*it) != 0;
  }

  bool path_is_valid = partial_path.IsValid();

  const ParsedCertificate* trusted_cert = partial_path.GetTrustedCert();
  if (trusted_cert) {
    if (!verify_result->is_issued_by_known_root) {
      verify_result->is_issued_by_known_root =
          trust_store->IsKnownRoot(trusted_cert);
    }

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
    LOG(ERROR) << "CertVerifyProcBuiltin for " << hostname << " failed:\n"
               << partial_path.errors.ToDebugString(partial_path.certs);
  }

  const PathBuilderDelegateDataImpl* delegate_data =
      PathBuilderDelegateDataImpl::Get(partial_path);
  if (delegate_data)
    verify_result->ocsp_result = delegate_data->stapled_ocsp_verify_result;

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
bool CanTryAgainWithWeakerDigestPolicy(const CertPathBuilder::Result& result) {
  return result.AnyPathContainsError(
      cert_errors::kUnacceptableSignatureAlgorithm);
}

int CertVerifyProcBuiltin::VerifyInternal(
    X509Certificate* input_cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  // VerifyInternal() is expected to carry out verifications using the current
  // time stamp.
  base::Time verification_time = base::Time::Now();
  base::TimeTicks deadline = base::TimeTicks::Now() + kMaxVerificationTime;
  der::GeneralizedTime der_verification_time;
  if (!EncodeTimeAsGeneralizedTime(verification_time, &der_verification_time)) {
    // This shouldn't be possible.
    // We don't really have a good error code for this type of error.
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    return ERR_CERT_AUTHORITY_INVALID;
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

  // Parse the target certificate.
  std::shared_ptr<const ParsedCertificate> target;
  {
    CertErrors parsing_errors;
    target =
        ParseCertificateFromBuffer(input_cert->cert_buffer(), &parsing_errors);
    // TODO(crbug.com/634484): this duplicates the logging of the input chain
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
  CertIssuerSourceStatic intermediates;
  AddIntermediatesToIssuerSource(input_cert, &intermediates, net_log);

  // Parse the additional trust anchors and setup trust store.
  CertVerifyProcTrustStore trust_store(system_trust_store_.get());
  for (const auto& x509_cert : additional_trust_anchors) {
    CertErrors parsing_errors;
    std::shared_ptr<const ParsedCertificate> cert =
        ParseCertificateFromBuffer(x509_cert->cert_buffer(), &parsing_errors);
    if (cert)
      trust_store.AddTrustAnchor(std::move(cert));
    // TODO(crbug.com/634484): this duplicates the logging of the
    // additional_trust_anchors maybe should only log if there is a parse
    // error/warning?
    net_log.AddEvent(
        NetLogEventType::CERT_VERIFY_PROC_ADDITIONAL_TRUST_ANCHOR, [&] {
          return NetLogCertParams(x509_cert->cert_buffer(), parsing_errors);
        });
  }

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
    attempts.emplace_back(VerificationType::kEV);

  // Next try DV validation.
  attempts.emplace_back(VerificationType::kDV);

  CertPathBuilder::Result result;
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
        target, &intermediates, &trust_store, der_verification_time, deadline,
        cur_attempt.verification_type, cur_attempt.digest_policy, flags,
        ocsp_response, crl_set(), net_fetcher_.get(), ev_metadata,
        &checked_revocation_for_some_path);

    base::UmaHistogramCounts10000("Net.CertVerifier.PathBuilderIterationCount",
                                  result.iteration_count);

    // TODO(crbug.com/634484): Log these in path_builder.cc so they include
    // correct timing information.
    for (const auto& path : result.paths) {
      net_log.AddEvent(NetLogEventType::CERT_VERIFY_PROC_PATH_BUILT,
                       [&] { return NetLogPathBuilderResultPath(*path); });
    }

    net_log.EndEvent(NetLogEventType::CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT,
                     [&] { return NetLogPathBuilderResult(result); });

    if (result.HasValidPath())
      break;

    if (result.exceeded_deadline) {
      // Stop immediately if an attempt exceeds the deadline.
      break;
    }

    // If this path building attempt (may have) failed due to the chain using a
    // weak signature algorithm, enqueue a similar attempt but with weaker
    // signature algorithms (SHA1) permitted.
    //
    // This fallback is necessary because the CertVerifyProc layer may decide to
    // allow SHA1 based on its own policy, so path building should return
    // possibly weak chains too.
    //
    // TODO(eroman): Would be better for the SHA1 policy to be part of the
    // delegate instead so it can interact with path building.
    if (cur_attempt.digest_policy ==
            SimplePathBuilderDelegate::DigestPolicy::kStrong &&
        CanTryAgainWithWeakerDigestPolicy(result)) {
      BuildPathAttempt sha1_fallback_attempt = cur_attempt;
      sha1_fallback_attempt.digest_policy =
          SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1;
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
    std::unique_ptr<SystemTrustStore> system_trust_store) {
  return base::MakeRefCounted<CertVerifyProcBuiltin>(
      std::move(net_fetcher), std::move(crl_set),
      std::move(system_trust_store));
}

base::TimeDelta GetCertVerifyProcBuiltinTimeLimitForTesting() {
  return kMaxVerificationTime;
}

}  // namespace net
