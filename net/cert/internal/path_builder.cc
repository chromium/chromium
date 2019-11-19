// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/path_builder.h"

#include <memory>
#include <set>
#include <unordered_set>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/internal/cert_issuer_source.h"
#include "net/cert/internal/certificate_policies.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parse_name.h"  // For CertDebugString.
#include "net/cert/internal/trust_store.h"
#include "net/cert/internal/verify_certificate_chain.h"
#include "net/cert/internal/verify_name_match.h"
#include "net/der/parser.h"
#include "net/der/tag.h"

namespace net {

namespace {

using CertIssuerSources = std::vector<CertIssuerSource*>;

// Returns a hex-encoded sha256 of the DER-encoding of |cert|.
std::string FingerPrintParsedCertificate(const net::ParsedCertificate* cert) {
  std::string hash = crypto::SHA256HashString(cert->der_cert().AsStringPiece());
  return base::HexEncode(hash.data(), hash.size());
}

// TODO(mattm): decide how much debug logging to keep.
std::string CertDebugString(const ParsedCertificate* cert) {
  RDNSequence subject;
  std::string subject_str;
  if (!ParseName(cert->tbs().subject_tlv, &subject) ||
      !ConvertToRFC2253(subject, &subject_str))
    subject_str = "???";

  return FingerPrintParsedCertificate(cert) + " " + subject_str;
}

std::string PathDebugString(const ParsedCertificateList& certs) {
  std::string s;
  for (const auto& cert : certs) {
    if (!s.empty())
      s += "\n";
    s += " " + CertDebugString(cert.get());
  }
  return s;
}

void RecordIterationCountHistogram(uint32_t iteration_count) {
  base::UmaHistogramCounts10000("Net.CertVerifier.PathBuilderIterationCount",
                                iteration_count);
}

// This structure describes a certificate and its trust level. Note that |cert|
// may be null to indicate an "empty" entry.
struct IssuerEntry {
  scoped_refptr<ParsedCertificate> cert;
  CertificateTrust trust;
  int trust_and_key_id_match_ordering;
};

enum KeyIdentifierMatch {
  // |target| has a keyIdentifier and it matches |issuer|'s
  // subjectKeyIdentifier.
  kMatch = 0,
  // |target| does not have authorityKeyIdentifier or |issuer| does not have
  // subjectKeyIdentifier.
  kNoData = 1,
  // |target|'s authorityKeyIdentifier does not match |issuer|.
  kMismatch = 2,
};

// Returns an integer that represents the relative ordering of |issuer| for
// prioritizing certificates in path building based on |issuer|'s
// subjectKeyIdentifier and |target|'s authorityKeyIdentifier. Lower return
// values indicate higer priority.
KeyIdentifierMatch CalculateKeyIdentifierMatch(
    const ParsedCertificate* target,
    const ParsedCertificate* issuer) {
  if (!target->authority_key_identifier())
    return kNoData;

  // TODO(crbug.com/635205): If issuer does not have a subjectKeyIdentifier,
  // could try synthesizing one using the standard SHA-1 method. Ideally in a
  // way where any issuers that do have a matching subjectKeyIdentifier could
  // be tried first before doing the extra work.
  if (target->authority_key_identifier()->key_identifier &&
      issuer->subject_key_identifier()) {
    if (target->authority_key_identifier()->key_identifier !=
        issuer->subject_key_identifier().value()) {
      return kMismatch;
    }
    return kMatch;
  }

  return kNoData;
}

// Returns an integer that represents the relative ordering of |issuer| based
// on |issuer_trust| and authorityKeyIdentifier matching for prioritizing
// certificates in path building. Lower return values indicate higer priority.
int TrustAndKeyIdentifierMatchToOrder(const ParsedCertificate* target,
                                      const ParsedCertificate* issuer,
                                      const CertificateTrust& issuer_trust) {
  enum {
    kTrustedAndKeyIdMatch = 0,
    kTrustedAndKeyIdNoData = 1,
    kKeyIdMatch = 2,
    kKeyIdNoData = 3,
    kTrustedAndKeyIdMismatch = 4,
    kKeyIdMismatch = 5,
    kDistrustedAndKeyIdMatch = 6,
    kDistrustedAndKeyIdNoData = 7,
    kDistrustedAndKeyIdMismatch = 8,
  };

  KeyIdentifierMatch key_id_match = CalculateKeyIdentifierMatch(target, issuer);
  switch (issuer_trust.type) {
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      switch (key_id_match) {
        case kMatch:
          return kTrustedAndKeyIdMatch;
        case kNoData:
          return kTrustedAndKeyIdNoData;
        case kMismatch:
          return kTrustedAndKeyIdMismatch;
      }
    case CertificateTrustType::UNSPECIFIED:
      switch (key_id_match) {
        case kMatch:
          return kKeyIdMatch;
        case kNoData:
          return kKeyIdNoData;
        case kMismatch:
          return kKeyIdMismatch;
      }
    case CertificateTrustType::DISTRUSTED:
      switch (key_id_match) {
        case kMatch:
          return kDistrustedAndKeyIdMatch;
        case kNoData:
          return kDistrustedAndKeyIdNoData;
        case kMismatch:
          return kDistrustedAndKeyIdMismatch;
      }
  }
}

// CertIssuersIter iterates through the intermediates from |cert_issuer_sources|
// which may be issuers of |cert|.
class CertIssuersIter {
 public:
  // Constructs the CertIssuersIter. |*cert_issuer_sources|, |*trust_store|,
  // and |*debug_data| must be valid for the lifetime of the CertIssuersIter.
  CertIssuersIter(scoped_refptr<ParsedCertificate> cert,
                  CertIssuerSources* cert_issuer_sources,
                  const TrustStore* trust_store,
                  base::SupportsUserData* debug_data);

  // Gets the next candidate issuer, or clears |*out| when all issuers have been
  // exhausted.
  void GetNextIssuer(IssuerEntry* out);

  // Returns the |cert| for which issuers are being retrieved.
  const ParsedCertificate* cert() const { return cert_.get(); }
  scoped_refptr<ParsedCertificate> reference_cert() const { return cert_; }

 private:
  void AddIssuers(ParsedCertificateList issuers);
  void DoAsyncIssuerQuery();

  // Returns true if |issuers_| contains unconsumed certificates.
  bool HasCurrentIssuer() const { return cur_issuer_ < issuers_.size(); }

  // Sorts the remaining entries in |issuers_| in the preferred order to
  // explore. Does not change the ordering for indices before cur_issuer_.
  void SortRemainingIssuers();

  scoped_refptr<ParsedCertificate> cert_;
  CertIssuerSources* cert_issuer_sources_;
  const TrustStore* trust_store_;

  // The list of issuers for |cert_|. This is added to incrementally (first
  // synchronous results, then possibly multiple times as asynchronous results
  // arrive.) The issuers may be re-sorted each time new issuers are added, but
  // only the results from |cur_| onwards should be sorted, since the earlier
  // results were already returned.
  // Elements should not be removed from |issuers_| once added, since
  // |present_issuers_| will point to data owned by the certs.
  std::vector<IssuerEntry> issuers_;
  // The index of the next cert in |issuers_| to return.
  size_t cur_issuer_ = 0;
  // Set to true whenever new issuers are appended at the end, to indicate the
  // ordering needs to be checked.
  bool issuers_needs_sort_ = false;

  // Set of DER-encoded values for the certs in |issuers_|. Used to prevent
  // duplicates. This is based on the full DER of the cert to allow different
  // versions of the same certificate to be tried in different candidate paths.
  // This points to data owned by |issuers_|.
  std::unordered_set<base::StringPiece, base::StringPieceHash> present_issuers_;

  // Tracks which requests have been made yet.
  bool did_initial_query_ = false;
  bool did_async_issuer_query_ = false;
  // Index into pending_async_requests_ that is the next one to process.
  size_t cur_async_request_ = 0;
  // Owns the Request objects for any asynchronous requests so that they will be
  // cancelled if CertIssuersIter is destroyed.
  std::vector<std::unique_ptr<CertIssuerSource::Request>>
      pending_async_requests_;

  base::SupportsUserData* debug_data_;

  DISALLOW_COPY_AND_ASSIGN(CertIssuersIter);
};

CertIssuersIter::CertIssuersIter(scoped_refptr<ParsedCertificate> in_cert,
                                 CertIssuerSources* cert_issuer_sources,
                                 const TrustStore* trust_store,
                                 base::SupportsUserData* debug_data)
    : cert_(in_cert),
      cert_issuer_sources_(cert_issuer_sources),
      trust_store_(trust_store),
      debug_data_(debug_data) {
  DVLOG(2) << "CertIssuersIter created for " << CertDebugString(cert());
}

void CertIssuersIter::GetNextIssuer(IssuerEntry* out) {
  if (!did_initial_query_) {
    did_initial_query_ = true;
    for (auto* cert_issuer_source : *cert_issuer_sources_) {
      ParsedCertificateList new_issuers;
      cert_issuer_source->SyncGetIssuersOf(cert(), &new_issuers);
      AddIssuers(std::move(new_issuers));
    }
  }

  // If there aren't any issuers, block until async results are ready.
  if (!HasCurrentIssuer()) {
    if (!did_async_issuer_query_) {
      // Now issue request(s) for async ones (AIA, etc).
      DoAsyncIssuerQuery();
    }

    // TODO(eroman): Rather than blocking on the async requests in FIFO order,
    // consume in the order they become ready.
    while (!HasCurrentIssuer() &&
           cur_async_request_ < pending_async_requests_.size()) {
      ParsedCertificateList new_issuers;
      pending_async_requests_[cur_async_request_]->GetNext(&new_issuers);
      if (new_issuers.empty()) {
        // Request is exhausted, no more results pending from that
        // CertIssuerSource.
        pending_async_requests_[cur_async_request_++].reset();
      } else {
        AddIssuers(std::move(new_issuers));
      }
    }
  }

  if (HasCurrentIssuer()) {
    SortRemainingIssuers();

    DVLOG(2) << "CertIssuersIter returning issuer " << cur_issuer_ << " of "
             << issuers_.size() << " for " << CertDebugString(cert());
    // Still have issuers that haven't been returned yet, return the highest
    // priority one (head of remaining list). A reference to the returned issuer
    // is retained, since |present_issuers_| points to data owned by it.
    *out = issuers_[cur_issuer_++];
    return;
  }

  DVLOG(2) << "CertIssuersIter reached the end of all available issuers for "
           << CertDebugString(cert());
  // Reached the end of all available issuers.
  *out = IssuerEntry();
}

void CertIssuersIter::AddIssuers(ParsedCertificateList new_issuers) {
  for (scoped_refptr<ParsedCertificate>& issuer : new_issuers) {
    if (present_issuers_.find(issuer->der_cert().AsStringPiece()) !=
        present_issuers_.end())
      continue;
    present_issuers_.insert(issuer->der_cert().AsStringPiece());

    // Look up the trust for this issuer.
    IssuerEntry entry;
    entry.cert = std::move(issuer);
    trust_store_->GetTrust(entry.cert, &entry.trust, debug_data_);
    entry.trust_and_key_id_match_ordering = TrustAndKeyIdentifierMatchToOrder(
        cert(), entry.cert.get(), entry.trust);

    issuers_.push_back(std::move(entry));
    issuers_needs_sort_ = true;
  }
}

void CertIssuersIter::DoAsyncIssuerQuery() {
  DCHECK(!did_async_issuer_query_);
  did_async_issuer_query_ = true;
  cur_async_request_ = 0;
  for (auto* cert_issuer_source : *cert_issuer_sources_) {
    std::unique_ptr<CertIssuerSource::Request> request;
    cert_issuer_source->AsyncGetIssuersOf(cert(), &request);
    if (request) {
      DVLOG(1) << "AsyncGetIssuersOf pending for " << CertDebugString(cert());
      pending_async_requests_.push_back(std::move(request));
    }
  }
}

void CertIssuersIter::SortRemainingIssuers() {
  if (!issuers_needs_sort_)
    return;

  std::stable_sort(
      issuers_.begin() + cur_issuer_, issuers_.end(),
      [](const IssuerEntry& issuer1, const IssuerEntry& issuer2) {
        // TODO(crbug.com/635205): Add other prioritization hints. (See big list
        // of possible sorting hints in RFC 4158.)
        return std::tie(issuer1.trust_and_key_id_match_ordering,
                        // Newer(larger) notBefore & notAfter dates are
                        // preferred, hence |issuer2| is on the LHS of
                        // the comparison and |issuer1| on the RHS.
                        issuer2.cert->tbs().validity_not_before,
                        issuer2.cert->tbs().validity_not_after) <
               std::tie(issuer2.trust_and_key_id_match_ordering,
                        issuer1.cert->tbs().validity_not_before,
                        issuer1.cert->tbs().validity_not_after);
      });

  issuers_needs_sort_ = false;
}

// CertIssuerIterPath tracks which certs are present in the path and prevents
// paths from being built which repeat any certs (including different versions
// of the same cert, based on Subject+SubjectAltName+SPKI).
// (RFC 5280 forbids duplicate certificates per section 6.1, and RFC 4158
// further recommends disallowing the same Subject+SubjectAltName+SPKI in
// section 2.4.2.)
class CertIssuerIterPath {
 public:
  // Returns true if |cert| is already present in the path.
  bool IsPresent(const ParsedCertificate* cert) const {
    return present_certs_.find(GetKey(cert)) != present_certs_.end();
  }

  // Appends |cert_issuers_iter| to the path. The cert referred to by
  // |cert_issuers_iter| must not be present in the path already.
  void Append(std::unique_ptr<CertIssuersIter> cert_issuers_iter) {
    bool added =
        present_certs_.insert(GetKey(cert_issuers_iter->cert())).second;
    DCHECK(added);
    cur_path_.push_back(std::move(cert_issuers_iter));
  }

  // Pops the last CertIssuersIter off the path.
  void Pop() {
    size_t num_erased = present_certs_.erase(GetKey(cur_path_.back()->cert()));
    DCHECK_EQ(num_erased, 1U);
    cur_path_.pop_back();
  }

  // Copies the ParsedCertificate elements of the current path to |*out_path|.
  void CopyPath(ParsedCertificateList* out_path) {
    out_path->clear();
    for (const auto& node : cur_path_)
      out_path->push_back(node->reference_cert());
  }

  // Returns true if the path is empty.
  bool Empty() const { return cur_path_.empty(); }

  // Returns the last CertIssuersIter in the path.
  CertIssuersIter* back() { return cur_path_.back().get(); }

  std::string PathDebugString() {
    std::string s;
    for (const auto& node : cur_path_) {
      if (!s.empty())
        s += "\n";
      s += " " + CertDebugString(node->cert());
    }
    return s;
  }

 private:
  using Key =
      std::tuple<base::StringPiece, base::StringPiece, base::StringPiece>;

  static Key GetKey(const ParsedCertificate* cert) {
    // TODO(mattm): ideally this would use a normalized version of
    // SubjectAltName, but it's not that important just for LoopChecker.
    //
    // Note that subject_alt_names_extension().value will be empty if the cert
    // had no SubjectAltName extension, so there is no need for a condition on
    // has_subject_alt_names().
    return Key(cert->normalized_subject().AsStringPiece(),
               cert->subject_alt_names_extension().value.AsStringPiece(),
               cert->tbs().spki_tlv.AsStringPiece());
  }

  std::vector<std::unique_ptr<CertIssuersIter>> cur_path_;

  // This refers to data owned by |cur_path_|.
  // TODO(mattm): use unordered_set. Requires making a hash function for Key.
  std::set<Key> present_certs_;
};

}  // namespace

const ParsedCertificate* CertPathBuilderResultPath::GetTrustedCert() const {
  if (certs.empty())
    return nullptr;

  switch (last_cert_trust.type) {
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
      return certs.back().get();
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::DISTRUSTED:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

// CertPathIter generates possible paths from |cert| to a trust anchor in
// |trust_store|, using intermediates from the |cert_issuer_source| objects if
// necessary.
class CertPathIter {
 public:
  CertPathIter(scoped_refptr<ParsedCertificate> cert,
               const TrustStore* trust_store,
               base::SupportsUserData* debug_data);

  // Adds a CertIssuerSource to provide intermediates for use in path building.
  // The |*cert_issuer_source| must remain valid for the lifetime of the
  // CertPathIter.
  void AddCertIssuerSource(CertIssuerSource* cert_issuer_source);

  // Gets the next candidate path, and fills it into |out_certs| and
  // |out_last_cert_trust|. Note that the returned path is unverified and must
  // still be run through a chain validator. Once all paths have been exhausted
  // returns false.
  bool GetNextPath(ParsedCertificateList* out_certs,
                   CertificateTrust* out_last_cert_trust,
                   const base::TimeTicks deadline,
                   uint32_t* iteration_count,
                   const uint32_t max_iteration_count);

 private:
  // Stores the next candidate issuer, until it is used during the
  // STATE_GET_NEXT_ISSUER_COMPLETE step.
  IssuerEntry next_issuer_;
  // The current path being explored, made up of CertIssuerIters. Each node
  // keeps track of the state of searching for issuers of that cert, so that
  // when backtracking it can resume the search where it left off.
  CertIssuerIterPath cur_path_;
  // The CertIssuerSources for retrieving candidate issuers.
  CertIssuerSources cert_issuer_sources_;
  // The TrustStore for checking if a path ends in a trust anchor.
  const TrustStore* trust_store_;

  base::SupportsUserData* debug_data_;

  DISALLOW_COPY_AND_ASSIGN(CertPathIter);
};

CertPathIter::CertPathIter(scoped_refptr<ParsedCertificate> cert,
                           const TrustStore* trust_store,
                           base::SupportsUserData* debug_data)
    : trust_store_(trust_store), debug_data_(debug_data) {
  // Initialize |next_issuer_| to the target certificate.
  next_issuer_.cert = std::move(cert);
  trust_store_->GetTrust(next_issuer_.cert, &next_issuer_.trust, debug_data_);
}

void CertPathIter::AddCertIssuerSource(CertIssuerSource* cert_issuer_source) {
  cert_issuer_sources_.push_back(cert_issuer_source);
}

bool CertPathIter::GetNextPath(ParsedCertificateList* out_certs,
                               CertificateTrust* out_last_cert_trust,
                               const base::TimeTicks deadline,
                               uint32_t* iteration_count,
                               const uint32_t max_iteration_count) {
  while (true) {
    if (!deadline.is_null() && base::TimeTicks::Now() > deadline)
      return false;

    if (!next_issuer_.cert) {
      if (cur_path_.Empty()) {
        DVLOG(1) << "CertPathIter exhausted all paths...";
        return false;
      }

      (*iteration_count)++;
      if (max_iteration_count > 0 && *iteration_count > max_iteration_count) {
        return false;
      }

      cur_path_.back()->GetNextIssuer(&next_issuer_);
      if (!next_issuer_.cert) {
        // TODO(mattm): should also include such paths in
        // CertPathBuilder::Result, maybe with a flag to enable it. Or use a
        // visitor pattern so the caller can decide what to do with any failed
        // paths. No more issuers for current chain, go back up and see if there
        // are any more for the previous cert.
        DVLOG(1) << "CertPathIter backtracking...";
        cur_path_.Pop();
        // Continue exploring issuers of the previous path...
        continue;
      }
    }

    // If the cert is trusted but is the leaf, treat it as having unspecified
    // trust. This may allow a successful path to be built to a different root
    // (or to the same cert if it's self-signed).
    switch (next_issuer_.trust.type) {
      case CertificateTrustType::TRUSTED_ANCHOR:
      case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS:
        if (cur_path_.Empty()) {
          DVLOG(1) << "Leaf is a trust anchor, considering as UNSPECIFIED";
          next_issuer_.trust = CertificateTrust::ForUnspecified();
        }
        break;
      case CertificateTrustType::DISTRUSTED:
      case CertificateTrustType::UNSPECIFIED:
        // No override necessary.
        break;
    }

    switch (next_issuer_.trust.type) {
      // If the trust for this issuer is "known" (either becuase it is
      // distrusted, or because it is trusted) then stop building and return the
      // path.
      case CertificateTrustType::DISTRUSTED:
      case CertificateTrustType::TRUSTED_ANCHOR:
      case CertificateTrustType::TRUSTED_ANCHOR_WITH_CONSTRAINTS: {
        // If the issuer has a known trust level, can stop building the path.
        DVLOG(2) << "CertPathIter got anchor: "
                 << CertDebugString(next_issuer_.cert.get());
        cur_path_.CopyPath(out_certs);
        out_certs->push_back(std::move(next_issuer_.cert));
        DVLOG(1) << "CertPathIter returning path:\n"
                 << PathDebugString(*out_certs);
        *out_last_cert_trust = next_issuer_.trust;
        next_issuer_ = IssuerEntry();
        return true;
      }
      case CertificateTrustType::UNSPECIFIED: {
        // Skip this cert if it is already in the chain.
        if (cur_path_.IsPresent(next_issuer_.cert.get())) {
          DVLOG(1) << "CertPathIter skipping dupe cert: "
                   << CertDebugString(next_issuer_.cert.get());
          next_issuer_ = IssuerEntry();
          continue;
        }

        cur_path_.Append(std::make_unique<CertIssuersIter>(
            std::move(next_issuer_.cert), &cert_issuer_sources_, trust_store_,
            debug_data_));
        next_issuer_ = IssuerEntry();
        DVLOG(1) << "CertPathIter cur_path_ =\n" << cur_path_.PathDebugString();
        // Continue descending the tree.
        continue;
      }
    }
  }
}

CertPathBuilderResultPath::CertPathBuilderResultPath() = default;
CertPathBuilderResultPath::~CertPathBuilderResultPath() = default;

bool CertPathBuilderResultPath::IsValid() const {
  return GetTrustedCert() && !errors.ContainsHighSeverityErrors();
}

CertPathBuilder::Result::Result() = default;
CertPathBuilder::Result::Result(Result&&) = default;
CertPathBuilder::Result::~Result() = default;
CertPathBuilder::Result& CertPathBuilder::Result::operator=(Result&&) = default;

bool CertPathBuilder::Result::HasValidPath() const {
  return GetBestValidPath() != nullptr;
}

bool CertPathBuilder::Result::AnyPathContainsError(CertErrorId error_id) const {
  for (const auto& path : paths) {
    if (path->errors.ContainsError(error_id))
      return true;
  }

  return false;
}

const CertPathBuilderResultPath* CertPathBuilder::Result::GetBestValidPath()
    const {
  const CertPathBuilderResultPath* result_path = GetBestPathPossiblyInvalid();

  if (result_path && result_path->IsValid())
    return result_path;

  return nullptr;
}

const CertPathBuilderResultPath*
CertPathBuilder::Result::GetBestPathPossiblyInvalid() const {
  DCHECK((paths.empty() && best_result_index == 0) ||
         best_result_index < paths.size());

  if (best_result_index >= paths.size())
    return nullptr;

  return paths[best_result_index].get();
}

CertPathBuilder::CertPathBuilder(
    scoped_refptr<ParsedCertificate> cert,
    TrustStore* trust_store,
    CertPathBuilderDelegate* delegate,
    const der::GeneralizedTime& time,
    KeyPurpose key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input>& user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit)
    : cert_path_iter_(new CertPathIter(std::move(cert),
                                       trust_store,
                                       /*debug_data=*/&out_result_)),
      delegate_(delegate),
      time_(time),
      key_purpose_(key_purpose),
      initial_explicit_policy_(initial_explicit_policy),
      user_initial_policy_set_(user_initial_policy_set),
      initial_policy_mapping_inhibit_(initial_policy_mapping_inhibit),
      initial_any_policy_inhibit_(initial_any_policy_inhibit) {
  DCHECK(delegate);
  // The TrustStore also implements the CertIssuerSource interface.
  AddCertIssuerSource(trust_store);
}

CertPathBuilder::~CertPathBuilder() = default;

void CertPathBuilder::AddCertIssuerSource(
    CertIssuerSource* cert_issuer_source) {
  cert_path_iter_->AddCertIssuerSource(cert_issuer_source);
}

void CertPathBuilder::SetIterationLimit(uint32_t limit) {
  max_iteration_count_ = limit;
}

void CertPathBuilder::SetDeadline(base::TimeTicks deadline) {
  deadline_ = deadline;
}

void CertPathBuilder::SetExploreAllPaths(bool explore_all_paths) {
  explore_all_paths_ = explore_all_paths;
}

CertPathBuilder::Result CertPathBuilder::Run() {
  uint32_t iteration_count = 0;

  while (true) {
    std::unique_ptr<CertPathBuilderResultPath> result_path =
        std::make_unique<CertPathBuilderResultPath>();

    if (!cert_path_iter_->GetNextPath(&result_path->certs,
                                      &result_path->last_cert_trust, deadline_,
                                      &iteration_count, max_iteration_count_)) {
      // No more paths to check.
      if (max_iteration_count_ > 0 && iteration_count > max_iteration_count_) {
        out_result_.exceeded_iteration_limit = true;
      }
      if (!deadline_.is_null() && base::TimeTicks::Now() > deadline_) {
        out_result_.exceeded_deadline = true;
      }
      RecordIterationCountHistogram(iteration_count);
      return std::move(out_result_);
    }

    // Verify the entire certificate chain.
    VerifyCertificateChain(
        result_path->certs, result_path->last_cert_trust, delegate_, time_,
        key_purpose_, initial_explicit_policy_, user_initial_policy_set_,
        initial_policy_mapping_inhibit_, initial_any_policy_inhibit_,
        &result_path->user_constrained_policy_set, &result_path->errors);

    DVLOG(1) << "CertPathBuilder VerifyCertificateChain errors:\n"
             << result_path->errors.ToDebugString(result_path->certs);

    // Give the delegate a chance to add errors to the path.
    delegate_->CheckPathAfterVerification(*this, result_path.get());

    bool path_is_good = result_path->IsValid();

    AddResultPath(std::move(result_path));

    if (path_is_good && !explore_all_paths_) {
      RecordIterationCountHistogram(iteration_count);
      // Found a valid path, return immediately.
      return std::move(out_result_);
    }
    // Path did not verify. Try more paths.
  }
}

void CertPathBuilder::AddResultPath(
    std::unique_ptr<CertPathBuilderResultPath> result_path) {
  // TODO(mattm): If there are no valid paths, set best_result_index based on
  // number or severity of errors. If there are multiple valid paths, could set
  // best_result_index based on prioritization (since due to AIA and such, the
  // actual order results were discovered may not match the ideal).
  if (result_path->IsValid() && !out_result_.HasValidPath())
    out_result_.best_result_index = out_result_.paths.size();
  out_result_.paths.push_back(std::move(result_path));
}

}  // namespace net
