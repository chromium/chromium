// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/symantec_certs.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/dns_util.h"
#include "net/extras/preload_data/decoder.h"
#include "net/http/http_security_headers.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"

namespace net {

const base::Feature kEnforceCTForNewCerts{"EnforceCTForNewCerts",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

#include "net/http/transport_security_state_ct_policies.inc"

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
#include "net/http/transport_security_state_static.h"  // nogncheck
// Points to the active transport security state source.
const TransportSecurityStateSource* const kDefaultHSTSSource = &kHSTSSource;
#else
const TransportSecurityStateSource* const kDefaultHSTSSource = nullptr;
#endif

const TransportSecurityStateSource* g_hsts_source = kDefaultHSTSSource;

// Parameters for remembering sent HPKP and Expect-CT reports.
const size_t kMaxReportCacheEntries = 50;
const int kTimeToRememberReportsMins = 60;
const size_t kReportCacheKeyLength = 16;

// Override for CheckCTRequirements() for unit tests. Possible values:
//  -1: Unless a delegate says otherwise, do not require CT.
//   0: Use the default implementation (e.g. production)
//   1: Unless a delegate says otherwise, require CT.
int g_ct_required_for_testing = 0;

// The date (as the number of seconds since the Unix Epoch) to enforce CT for
// new certificates.
constexpr base::FeatureParam<int> kEnforceCTForNewCertsDate{
    &kEnforceCTForNewCerts, "date", 0};

bool IsDynamicExpectCTEnabled() {
  return base::FeatureList::IsEnabled(
      TransportSecurityState::kDynamicExpectCTFeature);
}

void RecordUMAForHPKPReportFailure(const GURL& report_uri,
                                   int net_error,
                                   int http_response_code) {
  base::UmaHistogramSparse("Net.PublicKeyPinReportSendingFailure2", -net_error);
}

std::unique_ptr<base::ListValue> GetPEMEncodedChainAsList(
    const net::X509Certificate* cert_chain) {
  if (!cert_chain)
    return std::make_unique<base::ListValue>();

  std::unique_ptr<base::ListValue> result(new base::ListValue());
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  for (const std::string& cert : pem_encoded_chain)
    result->Append(std::make_unique<base::Value>(cert));

  return result;
}

bool HashReportForCache(const base::DictionaryValue& report,
                        const GURL& report_uri,
                        std::string* cache_key) {
  char hashed[crypto::kSHA256Length];
  std::string to_hash;
  if (!base::JSONWriter::Write(report, &to_hash))
    return false;
  to_hash += "," + report_uri.spec();
  crypto::SHA256HashString(to_hash, hashed, sizeof(hashed));
  static_assert(kReportCacheKeyLength <= sizeof(hashed),
                "HPKP report cache key size is larger than hash size.");
  *cache_key = std::string(hashed, kReportCacheKeyLength);
  return true;
}

bool GetHPKPReport(const HostPortPair& host_port_pair,
                   const TransportSecurityState::PKPState& pkp_state,
                   const X509Certificate* served_certificate_chain,
                   const X509Certificate* validated_certificate_chain,
                   std::string* serialized_report,
                   std::string* cache_key) {
  if (pkp_state.report_uri.is_empty())
    return false;

  base::DictionaryValue report;
  base::Time now = base::Time::Now();
  report.SetString("hostname", host_port_pair.host());
  report.SetInteger("port", host_port_pair.port());
  report.SetBoolean("include-subdomains", pkp_state.include_subdomains);
  report.SetString("noted-hostname", pkp_state.domain);

  std::unique_ptr<base::ListValue> served_certificate_chain_list =
      GetPEMEncodedChainAsList(served_certificate_chain);
  std::unique_ptr<base::ListValue> validated_certificate_chain_list =
      GetPEMEncodedChainAsList(validated_certificate_chain);
  report.Set("served-certificate-chain",
             std::move(served_certificate_chain_list));
  report.Set("validated-certificate-chain",
             std::move(validated_certificate_chain_list));

  std::unique_ptr<base::ListValue> known_pin_list(new base::ListValue());
  for (const auto& hash_value : pkp_state.spki_hashes) {
    std::string known_pin;

    switch (hash_value.tag()) {
      case HASH_VALUE_SHA256:
        known_pin += "pin-sha256=";
        break;
      default:
        // Don't bother reporting about hash types we don't support. SHA-256 is
        // the only standardized hash function for HPKP anyway.
        continue;
    }

    std::string base64_value;
    base::Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(hash_value.data()),
                          hash_value.size()),
        &base64_value);
    known_pin += "\"" + base64_value + "\"";

    known_pin_list->Append(
        std::unique_ptr<base::Value>(new base::Value(known_pin)));
  }

  report.Set("known-pins", std::move(known_pin_list));

  // For the sent reports cache, do not include the effective expiration
  // date. The expiration date will likely change every time the user
  // visits the site, so it would prevent reports from being effectively
  // deduplicated.
  if (!HashReportForCache(report, pkp_state.report_uri, cache_key)) {
    LOG(ERROR) << "Failed to compute cache key for HPKP violation report.";
    return false;
  }

  report.SetString("date-time", base::TimeToISO8601(now));
  report.SetString("effective-expiration-date",
                   base::TimeToISO8601(pkp_state.expiry));
  if (!base::JSONWriter::Write(report, serialized_report)) {
    LOG(ERROR) << "Failed to serialize HPKP violation report.";
    return false;
  }

  return true;
}

// Do not send a report over HTTPS to the same host that set the
// pin. Such report URIs will result in loops. (A.com has a pinning
// violation which results in a report being sent to A.com, which
// results in a pinning violation which results in a report being sent
// to A.com, etc.)
bool IsReportUriValidForHost(const GURL& report_uri, const std::string& host) {
  return (report_uri.host_piece() != host ||
          !report_uri.SchemeIsCryptographic());
}

std::string HashesToBase64String(const HashValueVector& hashes) {
  std::string str;
  for (size_t i = 0; i != hashes.size(); ++i) {
    if (i != 0)
      str += ",";
    str += hashes[i].ToString();
  }
  return str;
}

std::string HashHost(const std::string& canonicalized_host) {
  char hashed[crypto::kSHA256Length];
  crypto::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));
  return std::string(hashed, sizeof(hashed));
}

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
bool HashesIntersect(const HashValueVector& a,
                     const HashValueVector& b) {
  for (const auto& hash : a) {
    if (base::Contains(b, hash))
      return true;
  }
  return false;
}

bool AddHash(const char* sha256_hash, HashValueVector* out) {
  HashValue hash(HASH_VALUE_SHA256);
  memcpy(hash.data(), sha256_hash, hash.size());
  out->push_back(hash);
  return true;
}

// Converts |hostname| from dotted form ("www.google.com") to the form
// used in DNS: "\x03www\x06google\x03com", lowercases that, and returns
// the result.
std::string CanonicalizeHost(const std::string& host) {
  // We cannot perform the operations as detailed in the spec here as |host|
  // has already undergone IDN processing before it reached us. Thus, we check
  // that there are no invalid characters in the host and lowercase the result.
  std::string new_host;
  if (!DNSDomainFromDot(host, &new_host)) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::string();
  }

  for (size_t i = 0; new_host[i]; i += new_host[i] + 1) {
    const unsigned label_length = static_cast<unsigned>(new_host[i]);
    if (!label_length)
      break;

    for (size_t j = 0; j < label_length; ++j) {
      new_host[i + 1 + j] = static_cast<char>(tolower(new_host[i + 1 + j]));
    }
  }

  return new_host;
}

// PreloadResult is the result of resolving a specific name in the preloaded
// data.
struct PreloadResult {
  uint32_t pinset_id = 0;
  // hostname_offset contains the number of bytes from the start of the given
  // hostname where the name of the matching entry starts.
  size_t hostname_offset = 0;
  bool sts_include_subdomains = false;
  bool pkp_include_subdomains = false;
  bool force_https = false;
  bool has_pins = false;
  bool expect_ct = false;
  uint32_t expect_ct_report_uri_id = 0;
};

using net::extras::PreloadDecoder;

// Extracts the current PreloadResult entry from the given Huffman encoded trie.
// If an "end of string" matches a period in the hostname then the information
// is remembered because, if no more specific node is found, then that
// information applies to the hostname.
class HSTSPreloadDecoder : public net::extras::PreloadDecoder {
 public:
  using net::extras::PreloadDecoder::PreloadDecoder;

  // net::extras::PreloadDecoder:
  bool ReadEntry(net::extras::PreloadDecoder::BitReader* reader,
                 const std::string& search,
                 size_t current_search_offset,
                 bool* out_found) override {
    bool is_simple_entry;
    if (!reader->Next(&is_simple_entry)) {
      return false;
    }
    PreloadResult tmp;
    // Simple entries only configure HSTS with IncludeSubdomains and use a
    // compact serialization format where the other policy flags are
    // omitted. The omitted flags are assumed to be 0 and the associated
    // policies are disabled.
    if (is_simple_entry) {
      tmp.force_https = true;
      tmp.sts_include_subdomains = true;
    } else {
      if (!reader->Next(&tmp.sts_include_subdomains) ||
          !reader->Next(&tmp.force_https) || !reader->Next(&tmp.has_pins)) {
        return false;
      }

      tmp.pkp_include_subdomains = tmp.sts_include_subdomains;

      if (tmp.has_pins) {
        if (!reader->Read(4, &tmp.pinset_id) ||
            (!tmp.sts_include_subdomains &&
             !reader->Next(&tmp.pkp_include_subdomains))) {
          return false;
        }
      }

      if (!reader->Next(&tmp.expect_ct))
        return false;

      if (tmp.expect_ct) {
        if (!reader->Read(4, &tmp.expect_ct_report_uri_id))
          return false;
      }
    }

    tmp.hostname_offset = current_search_offset;

    if (current_search_offset == 0 ||
        search[current_search_offset - 1] == '.') {
      *out_found = tmp.sts_include_subdomains || tmp.pkp_include_subdomains;

      result_ = tmp;

      if (current_search_offset > 0) {
        result_.force_https &= tmp.sts_include_subdomains;
      } else {
        *out_found = true;
        return true;
      }
    }
    return true;
  }

  PreloadResult result() const { return result_; }

 private:
  PreloadResult result_;
};

bool DecodeHSTSPreload(const std::string& search_hostname, PreloadResult* out) {
#if !BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
  if (g_hsts_source == nullptr)
    return false;
#endif
  bool found = false;

  // Ensure that |search_hostname| is a valid hostname before
  // processing.
  if (CanonicalizeHost(search_hostname).empty()) {
    return false;
  }
  // Normalize any trailing '.' used for DNS suffix searches.
  std::string hostname = search_hostname;
  size_t trailing_dot_found = hostname.find_last_not_of('.');
  if (trailing_dot_found != std::string::npos) {
    hostname.erase(trailing_dot_found + 1);
  } else {
    hostname.clear();
  }

  // |hostname| has already undergone IDN conversion, so should be
  // entirely A-Labels. The preload data is entirely normalized to
  // lower case.
  hostname = base::ToLowerASCII(hostname);
  if (hostname.empty()) {
    return false;
  }

  HSTSPreloadDecoder decoder(
      g_hsts_source->huffman_tree, g_hsts_source->huffman_tree_size,
      g_hsts_source->preloaded_data, g_hsts_source->preloaded_bits,
      g_hsts_source->root_position);
  if (!decoder.Decode(hostname, &found)) {
    DCHECK(false) << "Internal error in DecodeHSTSPreload for hostname "
                  << hostname;
    return false;
  }
  if (found)
    *out = decoder.result();
  return found;
}

}  // namespace

// static
const base::Feature TransportSecurityState::kDynamicExpectCTFeature{
    "DynamicExpectCT", base::FEATURE_ENABLED_BY_DEFAULT};

void SetTransportSecurityStateSourceForTesting(
    const TransportSecurityStateSource* source) {
  g_hsts_source = source ? source : kDefaultHSTSSource;
}

TransportSecurityState::TransportSecurityState()
    : TransportSecurityState(std::vector<std::string>()) {}

TransportSecurityState::TransportSecurityState(
    std::vector<std::string> hsts_host_bypass_list)
    : enable_static_pins_(true),
      enable_static_expect_ct_(true),
      enable_pkp_bypass_for_local_trust_anchors_(true),
      sent_hpkp_reports_cache_(kMaxReportCacheEntries),
      sent_expect_ct_reports_cache_(kMaxReportCacheEntries) {
// Static pinning is only enabled for official builds to make sure that
// others don't end up with pins that cannot be easily updated.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || defined(OS_ANDROID) || defined(OS_IOS)
  enable_static_pins_ = false;
  enable_static_expect_ct_ = false;
#endif
  // Check that there no invalid entries in the static HSTS bypass list.
  for (auto& host : hsts_host_bypass_list) {
    DCHECK(host.find('.') == std::string::npos);
    hsts_host_bypass_list_.insert(host);
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// Both HSTS and HPKP cause fatal SSL errors, so return true if a
// host has either.
bool TransportSecurityState::ShouldSSLErrorsBeFatal(const std::string& host) {
  STSState unused_sts;
  PKPState unused_pkp;
  return GetStaticDomainState(host, &unused_sts, &unused_pkp) ||
         GetDynamicSTSState(host, &unused_sts) ||
         GetDynamicPKPState(host, &unused_pkp);
}

bool TransportSecurityState::ShouldUpgradeToSSL(const std::string& host) {
  STSState sts_state;
  return GetSTSState(host, &sts_state) && sts_state.ShouldUpgradeToSSL();
}

TransportSecurityState::PKPStatus TransportSecurityState::CheckPublicKeyPins(
    const HostPortPair& host_port_pair,
    bool is_issued_by_known_root,
    const HashValueVector& public_key_hashes,
    const X509Certificate* served_certificate_chain,
    const X509Certificate* validated_certificate_chain,
    const PublicKeyPinReportStatus report_status,
    std::string* pinning_failure_log) {
  // Perform pin validation only if the server actually has public key pins.
  if (!HasPublicKeyPins(host_port_pair.host())) {
    return PKPStatus::OK;
  }

  PKPStatus pin_validity = CheckPublicKeyPinsImpl(
      host_port_pair, is_issued_by_known_root, public_key_hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      pinning_failure_log);

  // Don't track statistics when a local trust anchor would override the pinning
  // anyway.
  if (!is_issued_by_known_root)
    return pin_validity;

  UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess",
                        pin_validity == PKPStatus::OK);
  return pin_validity;
}

bool TransportSecurityState::HasPublicKeyPins(const std::string& host) {
  PKPState pkp_state;
  return GetPKPState(host, &pkp_state) && pkp_state.HasPublicKeyPins();
}

TransportSecurityState::CTRequirementsStatus
TransportSecurityState::CheckCTRequirements(
    const net::HostPortPair& host_port_pair,
    bool is_issued_by_known_root,
    const HashValueVector& public_key_hashes,
    const X509Certificate* validated_certificate_chain,
    const X509Certificate* served_certificate_chain,
    const SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps,
    const ExpectCTReportStatus report_status,
    ct::CTPolicyCompliance policy_compliance) {
  using CTRequirementLevel = RequireCTDelegate::CTRequirementLevel;
  std::string hostname = host_port_pair.host();

  // CT is not required if the certificate does not chain to a publicly
  // trusted root certificate. Testing can override this, as certain tests
  // rely on using a non-publicly-trusted root.
  if (!is_issued_by_known_root && g_ct_required_for_testing == 0)
    return CT_NOT_REQUIRED;

  // A connection is considered compliant if it has sufficient SCTs or if the
  // build is outdated. Other statuses are not considered compliant; this
  // includes COMPLIANCE_DETAILS_NOT_AVAILABLE because compliance must have been
  // evaluated in order to determine that the connection is compliant.
  bool complies =
      (policy_compliance ==
           ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
       policy_compliance == ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY);

  // Check Expect-CT first so that other CT requirements do not prevent
  // Expect-CT reports from being sent.
  bool required_via_expect_ct = false;
  ExpectCTState state;
  if (IsDynamicExpectCTEnabled() && GetDynamicExpectCTState(hostname, &state)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.ExpectCTHeader.PolicyComplianceOnConnectionSetup",
        policy_compliance, ct::CTPolicyCompliance::CT_POLICY_COUNT);
    if (!complies && expect_ct_reporter_ && !state.report_uri.is_empty() &&
        report_status == ENABLE_EXPECT_CT_REPORTS) {
      MaybeNotifyExpectCTFailed(host_port_pair, state.report_uri, state.expiry,
                                validated_certificate_chain,
                                served_certificate_chain,
                                signed_certificate_timestamps);
    }
    required_via_expect_ct = state.enforce;
  }

  CTRequirementLevel ct_required = CTRequirementLevel::DEFAULT;
  if (require_ct_delegate_) {
    // Allow the delegate to override the CT requirement state, including
    // overriding any Expect-CT enforcement.
    ct_required = require_ct_delegate_->IsCTRequiredForHost(
        hostname, validated_certificate_chain, public_key_hashes);
  }
  switch (ct_required) {
    case CTRequirementLevel::REQUIRED:
      return complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET;
    case CTRequirementLevel::NOT_REQUIRED:
      return CT_NOT_REQUIRED;
    case CTRequirementLevel::DEFAULT:
      if (required_via_expect_ct) {
        // If Expect-CT is set, short-circuit checking additional policies,
        // since they will only enable CT requirement, not exclude from it.
        return complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET;
      }
      break;
  }

  // Allow unittests to override the default result.
  if (g_ct_required_for_testing)
    return (g_ct_required_for_testing == 1
                ? (complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET)
                : CT_NOT_REQUIRED);

  // This is provided as a means for CAs to test their own issuance practices
  // prior to Certificate Transparency becoming mandatory. A parameterized
  // Feature/FieldTrial is provided, with a single parameter, "date", that
  // allows a CA to simulate an enforcement date. The expected use case is
  // that a CA will simulate a date of today/yesterday to see if their newly
  // issued certificates comply.
  if (base::FeatureList::IsEnabled(kEnforceCTForNewCerts)) {
    base::Time enforcement_date =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromSeconds(kEnforceCTForNewCertsDate.Get());
    if (enforcement_date > base::Time::UnixEpoch() &&
        validated_certificate_chain->valid_start() > enforcement_date) {
      return complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET;
    }
  }

  const base::Time epoch = base::Time::UnixEpoch();
  const CTRequiredPolicies& ct_required_policies = GetCTRequiredPolicies();

  bool found = false;
  for (const auto& restricted_ca : ct_required_policies) {
    if (!restricted_ca.effective_date.is_zero() &&
        (epoch + restricted_ca.effective_date >
         validated_certificate_chain->valid_start())) {
      // The candidate cert is not subject to the CT policy, because it
      // was issued before the effective CT date.
      continue;
    }

    if (!IsAnySHA256HashInSortedArray(
            public_key_hashes,
            base::make_span(restricted_ca.roots, restricted_ca.roots_length))) {
      // No match for this set of restricted roots.
      continue;
    }

    // Found a match, indicating this certificate is potentially
    // restricted. Determine if any of the hashes are on the exclusion
    // list as exempt from the CT requirement.
    if (restricted_ca.exceptions &&
        IsAnySHA256HashInSortedArray(
            public_key_hashes,
            base::make_span(restricted_ca.exceptions,
                            restricted_ca.exceptions_length))) {
      // Found an excluded sub-CA; CT is not required.
      continue;
    }

    // No exception found. This certificate must conform to the CT policy. The
    // compliance state is treated as additive - it must comply with all
    // stated policies.
    found = true;
  }
  if (found)
    return complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET;

  return CT_NOT_REQUIRED;
}

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate;
}

void TransportSecurityState::SetReportSender(
    TransportSecurityState::ReportSenderInterface* report_sender) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  report_sender_ = report_sender;
}

void TransportSecurityState::SetExpectCTReporter(
    ExpectCTReporter* expect_ct_reporter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  expect_ct_reporter_ = expect_ct_reporter;
}

void TransportSecurityState::SetRequireCTDelegate(RequireCTDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  require_ct_delegate_ = delegate;
}

void TransportSecurityState::AddHSTSInternal(
    const std::string& host,
    TransportSecurityState::STSState::UpgradeMode upgrade_mode,
    const base::Time& expiry,
    bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  STSState sts_state;
  sts_state.last_observed = base::Time::Now();
  sts_state.include_subdomains = include_subdomains;
  sts_state.expiry = expiry;
  sts_state.upgrade_mode = upgrade_mode;

  EnableSTSHost(host, sts_state);
}

void TransportSecurityState::AddHPKPInternal(const std::string& host,
                                             const base::Time& last_observed,
                                             const base::Time& expiry,
                                             bool include_subdomains,
                                             const HashValueVector& hashes,
                                             const GURL& report_uri) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  PKPState pkp_state;
  pkp_state.last_observed = last_observed;
  pkp_state.expiry = expiry;
  pkp_state.include_subdomains = include_subdomains;
  pkp_state.spki_hashes = hashes;
  pkp_state.report_uri = report_uri;

  EnablePKPHost(host, pkp_state);
}

void TransportSecurityState::AddExpectCTInternal(
    const std::string& host,
    const base::Time& last_observed,
    const base::Time& expiry,
    bool enforce,
    const GURL& report_uri) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ExpectCTState expect_ct_state;
  expect_ct_state.last_observed = last_observed;
  expect_ct_state.expiry = expiry;
  expect_ct_state.enforce = enforce;
  expect_ct_state.report_uri = report_uri;

  EnableExpectCTHost(host, expect_ct_state);
}

void TransportSecurityState::
    SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value) {
  enable_pkp_bypass_for_local_trust_anchors_ = value;
}

void TransportSecurityState::EnableSTSHost(const std::string& host,
                                           const STSState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  // Only store new state when HSTS is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (state.ShouldUpgradeToSSL()) {
    STSState sts_state(state);
    // No need to store this value since it is redundant. (|canonicalized_host|
    // is the map key.)
    sts_state.domain.clear();

    enabled_sts_hosts_[HashHost(canonicalized_host)] = sts_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_sts_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

void TransportSecurityState::EnablePKPHost(const std::string& host,
                                           const PKPState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  // Only store new state when HPKP is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (state.HasPublicKeyPins()) {
    PKPState pkp_state(state);
    // No need to store this value since it is redundant. (|canonicalized_host|
    // is the map key.)
    pkp_state.domain.clear();

    enabled_pkp_hosts_[HashHost(canonicalized_host)] = pkp_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_pkp_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

void TransportSecurityState::EnableExpectCTHost(const std::string& host,
                                                const ExpectCTState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsDynamicExpectCTEnabled())
    return;

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  // Only store new state when Expect-CT is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (state.enforce || !state.report_uri.is_empty()) {
    ExpectCTState expect_ct_state(state);
    // No need to store this value since it is redundant. (|canonicalized_host|
    // is the map key.)
    expect_ct_state.domain.clear();

    enabled_expect_ct_hosts_[HashHost(canonicalized_host)] = expect_ct_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_expect_ct_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

TransportSecurityState::PKPStatus
TransportSecurityState::CheckPinsAndMaybeSendReport(
    const HostPortPair& host_port_pair,
    bool is_issued_by_known_root,
    const TransportSecurityState::PKPState& pkp_state,
    const HashValueVector& hashes,
    const X509Certificate* served_certificate_chain,
    const X509Certificate* validated_certificate_chain,
    const TransportSecurityState::PublicKeyPinReportStatus report_status,
    std::string* failure_log) {
  if (pkp_state.CheckPublicKeyPins(hashes, failure_log))
    return PKPStatus::OK;

  // Don't report violations for certificates that chain to local roots.
  if (!is_issued_by_known_root && enable_pkp_bypass_for_local_trust_anchors_)
    return PKPStatus::BYPASSED;

  if (!report_sender_ ||
      report_status != TransportSecurityState::ENABLE_PIN_REPORTS ||
      pkp_state.report_uri.is_empty()) {
    return PKPStatus::VIOLATED;
  }

  DCHECK(pkp_state.report_uri.is_valid());
  // Report URIs should not be used if they are the same host as the pin
  // and are HTTPS, to avoid going into a report-sending loop.
  if (!IsReportUriValidForHost(pkp_state.report_uri, host_port_pair.host()))
    return PKPStatus::VIOLATED;

  std::string serialized_report;
  std::string report_cache_key;
  if (!GetHPKPReport(host_port_pair, pkp_state, served_certificate_chain,
                     validated_certificate_chain, &serialized_report,
                     &report_cache_key)) {
    return PKPStatus::VIOLATED;
  }

  // Limit the rate at which duplicate reports are sent to the same
  // report URI. The same report will not be sent within
  // |kTimeToRememberReportsMins|, which reduces load on servers and
  // also prevents accidental loops (a.com triggers a report to b.com
  // which triggers a report to a.com). See section 2.1.4 of RFC 7469.
  if (sent_hpkp_reports_cache_.Get(report_cache_key, base::TimeTicks::Now()))
    return PKPStatus::VIOLATED;
  sent_hpkp_reports_cache_.Put(
      report_cache_key, true, base::TimeTicks::Now(),
      base::TimeTicks::Now() +
          base::TimeDelta::FromMinutes(kTimeToRememberReportsMins));

  report_sender_->Send(pkp_state.report_uri, "application/json; charset=utf-8",
                       serialized_report, base::Callback<void()>(),
                       base::Bind(RecordUMAForHPKPReportFailure));
  return PKPStatus::VIOLATED;
}

bool TransportSecurityState::GetStaticExpectCTState(
    const std::string& host,
    ExpectCTState* expect_ct_state) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsBuildTimely())
    return false;

  PreloadResult result;
  if (!DecodeHSTSPreload(host, &result))
    return false;

  if (!enable_static_expect_ct_ || !result.expect_ct)
    return false;

  expect_ct_state->domain = host.substr(result.hostname_offset);
  expect_ct_state->report_uri = GURL(
      g_hsts_source->expect_ct_report_uris[result.expect_ct_report_uri_id]);
  return true;
}

void TransportSecurityState::MaybeNotifyExpectCTFailed(
    const HostPortPair& host_port_pair,
    const GURL& report_uri,
    base::Time expiration,
    const X509Certificate* validated_certificate_chain,
    const X509Certificate* served_certificate_chain,
    const SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  // Do not send repeated reports to the same host/port pair within
  // |kTimeToRememberReportsMins|. Theoretically, there could be scenarios in
  // which the same host/port generates different reports and it would be useful
  // to the server operator to receive those different reports, but such
  // scenarios are not expected to arise very often in practice.
  const std::string report_cache_key(host_port_pair.ToString());
  if (sent_expect_ct_reports_cache_.Get(report_cache_key,
                                        base::TimeTicks::Now())) {
    return;
  }
  sent_expect_ct_reports_cache_.Put(
      report_cache_key, true, base::TimeTicks::Now(),
      base::TimeTicks::Now() +
          base::TimeDelta::FromMinutes(kTimeToRememberReportsMins));

  expect_ct_reporter_->OnExpectCTFailed(
      host_port_pair, report_uri, expiration, validated_certificate_chain,
      served_certificate_chain, signed_certificate_timestamps);
}

bool TransportSecurityState::DeleteDynamicDataForHost(const std::string& host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  const std::string hashed_host = HashHost(canonicalized_host);
  bool deleted = false;
  auto sts_interator = enabled_sts_hosts_.find(hashed_host);
  if (sts_interator != enabled_sts_hosts_.end()) {
    enabled_sts_hosts_.erase(sts_interator);
    deleted = true;
  }

  auto pkp_iterator = enabled_pkp_hosts_.find(hashed_host);
  if (pkp_iterator != enabled_pkp_hosts_.end()) {
    enabled_pkp_hosts_.erase(pkp_iterator);
    deleted = true;
  }

  auto expect_ct_iterator = enabled_expect_ct_hosts_.find(hashed_host);
  if (expect_ct_iterator != enabled_expect_ct_hosts_.end()) {
    enabled_expect_ct_hosts_.erase(expect_ct_iterator);
    deleted = true;
  }

  if (deleted)
    DirtyNotify();
  return deleted;
}

void TransportSecurityState::ClearDynamicData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  enabled_sts_hosts_.clear();
  enabled_pkp_hosts_.clear();
  enabled_expect_ct_hosts_.clear();
}

void TransportSecurityState::DeleteAllDynamicDataSince(
    const base::Time& time,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool dirtied = false;
  auto sts_iterator = enabled_sts_hosts_.begin();
  while (sts_iterator != enabled_sts_hosts_.end()) {
    if (sts_iterator->second.last_observed >= time) {
      dirtied = true;
      enabled_sts_hosts_.erase(sts_iterator++);
      continue;
    }

    ++sts_iterator;
  }

  auto pkp_iterator = enabled_pkp_hosts_.begin();
  while (pkp_iterator != enabled_pkp_hosts_.end()) {
    if (pkp_iterator->second.last_observed >= time) {
      dirtied = true;
      enabled_pkp_hosts_.erase(pkp_iterator++);
      continue;
    }

    ++pkp_iterator;
  }

  auto expect_ct_iterator = enabled_expect_ct_hosts_.begin();
  while (expect_ct_iterator != enabled_expect_ct_hosts_.end()) {
    if (expect_ct_iterator->second.last_observed >= time) {
      dirtied = true;
      enabled_expect_ct_hosts_.erase(expect_ct_iterator++);
      continue;
    }

    ++expect_ct_iterator;
  }

  if (dirtied && delegate_)
    delegate_->WriteNow(this, std::move(callback));
  else
    std::move(callback).Run();
}

TransportSecurityState::~TransportSecurityState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TransportSecurityState::DirtyNotify() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (delegate_)
    delegate_->StateIsDirty(this);
}

bool TransportSecurityState::AddHSTSHeader(const std::string& host,
                                           const std::string& value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  bool include_subdomains;
  if (!ParseHSTSHeader(value, &max_age, &include_subdomains)) {
    return false;
  }

  // Handle max-age == 0.
  STSState::UpgradeMode upgrade_mode;
  if (max_age.InSeconds() == 0) {
    upgrade_mode = STSState::MODE_DEFAULT;
  } else {
    upgrade_mode = STSState::MODE_FORCE_HTTPS;
  }

  AddHSTSInternal(host, upgrade_mode, now + max_age, include_subdomains);
  return true;
}

void TransportSecurityState::AddHSTS(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AddHSTSInternal(host, STSState::MODE_FORCE_HTTPS, expiry, include_subdomains);
}

void TransportSecurityState::AddHPKP(const std::string& host,
                                     const base::Time& expiry,
                                     bool include_subdomains,
                                     const HashValueVector& hashes,
                                     const GURL& report_uri) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AddHPKPInternal(host, base::Time::Now(), expiry, include_subdomains, hashes,
                  report_uri);
}

void TransportSecurityState::AddExpectCT(const std::string& host,
                                         const base::Time& expiry,
                                         bool enforce,
                                         const GURL& report_uri) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AddExpectCTInternal(host, base::Time::Now(), expiry, enforce, report_uri);
}

void TransportSecurityState::ProcessExpectCTHeader(
    const std::string& value,
    const HostPortPair& host_port_pair,
    const SSLInfo& ssl_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If a site sends `Expect-CT: preload` and appears on the preload list, they
  // are in the experimental preload-list-only, report-only version of
  // Expect-CT.
  if (value == "preload") {
    if (!expect_ct_reporter_)
      return;
    if (!ssl_info.is_issued_by_known_root)
      return;
    if (ssl_info.ct_policy_compliance ==
            ct::CTPolicyCompliance::
                CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE ||
        ssl_info.ct_policy_compliance ==
            ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS ||
        ssl_info.ct_policy_compliance ==
            ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY) {
      return;
    }
    ExpectCTState state;
    if (GetStaticExpectCTState(host_port_pair.host(), &state)) {
      MaybeNotifyExpectCTFailed(host_port_pair, state.report_uri, base::Time(),
                                ssl_info.cert.get(),
                                ssl_info.unverified_cert.get(),
                                ssl_info.signed_certificate_timestamps);
    }
    return;
  }

  // Otherwise, see if the site has sent a valid Expect-CT header to dynamically
  // turn on reporting and/or enforcement.
  if (!IsDynamicExpectCTEnabled())
    return;
  base::Time now = base::Time::Now();
  base::TimeDelta max_age;
  bool enforce;
  GURL report_uri;
  bool parsed = ParseExpectCTHeader(value, &max_age, &enforce, &report_uri);
  UMA_HISTOGRAM_BOOLEAN("Net.ExpectCTHeader.ParseSuccess", parsed);
  if (!parsed)
    return;
  // Do not persist Expect-CT headers if the connection was not chained to a
  // public root or did not comply with CT policy.
  if (!ssl_info.is_issued_by_known_root)
    return;
  UMA_HISTOGRAM_ENUMERATION(
      "Net.ExpectCTHeader.PolicyComplianceOnHeaderProcessing",
      ssl_info.ct_policy_compliance, ct::CTPolicyCompliance::CT_POLICY_COUNT);
  if (ssl_info.ct_policy_compliance !=
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS) {
    // If an Expect-CT header is observed over a non-compliant connection, the
    // site owner should be notified about the misconfiguration. If the site was
    // already opted in to Expect-CT, this report would have been sent at
    // connection setup time. If the host is not already a noted Expect-CT host,
    // however, the lack of CT compliance would not have been evaluated/reported
    // at connection setup time, so it needs to be reported here while
    // processing the header.
    if (ssl_info.ct_policy_compliance ==
            ct::CTPolicyCompliance::CT_POLICY_BUILD_NOT_TIMELY ||
        ssl_info.ct_policy_compliance ==
            ct::CTPolicyCompliance::
                CT_POLICY_COMPLIANCE_DETAILS_NOT_AVAILABLE) {
      // Only send reports for truly non-compliant connections, not those for
      // which compliance wasn't checked.
      return;
    }
    ExpectCTState state;
    if (expect_ct_reporter_ && !report_uri.is_empty() &&
        !GetDynamicExpectCTState(host_port_pair.host(), &state)) {
      MaybeNotifyExpectCTFailed(host_port_pair, report_uri, base::Time(),
                                ssl_info.cert.get(),
                                ssl_info.unverified_cert.get(),
                                ssl_info.signed_certificate_timestamps);
    }
    return;
  }
  AddExpectCTInternal(host_port_pair.host(), now, now + max_age, enforce,
                      report_uri);
}

// static
void TransportSecurityState::SetShouldRequireCTForTesting(bool* required) {
  if (!required) {
    g_ct_required_for_testing = 0;
    return;
  }
  g_ct_required_for_testing = *required ? 1 : -1;
}

void TransportSecurityState::ClearReportCachesForTesting() {
  sent_hpkp_reports_cache_.Clear();
  sent_expect_ct_reports_cache_.Clear();
}

// static
bool TransportSecurityState::IsBuildTimely() {
  const base::Time build_time = base::GetBuildTime();
  // We consider built-in information to be timely for 10 weeks.
  return (base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */;
}

TransportSecurityState::PKPStatus
TransportSecurityState::CheckPublicKeyPinsImpl(
    const HostPortPair& host_port_pair,
    bool is_issued_by_known_root,
    const HashValueVector& hashes,
    const X509Certificate* served_certificate_chain,
    const X509Certificate* validated_certificate_chain,
    const PublicKeyPinReportStatus report_status,
    std::string* failure_log) {
  PKPState pkp_state;
  bool found_state = GetPKPState(host_port_pair.host(), &pkp_state);

  // HasPublicKeyPins should have returned true in order for this method to have
  // been called.
  DCHECK(found_state);
  return CheckPinsAndMaybeSendReport(
      host_port_pair, is_issued_by_known_root, pkp_state, hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      failure_log);
}

bool TransportSecurityState::GetStaticDomainState(const std::string& host,
                                                  STSState* sts_result,
                                                  PKPState* pkp_result) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsBuildTimely())
    return false;

  PreloadResult result;
  if (!DecodeHSTSPreload(host, &result))
    return false;

  if (hsts_host_bypass_list_.find(host) == hsts_host_bypass_list_.end() &&
      result.force_https) {
    sts_result->domain = host.substr(result.hostname_offset);
    sts_result->include_subdomains = result.sts_include_subdomains;
    sts_result->last_observed = base::GetBuildTime();
    sts_result->upgrade_mode = STSState::MODE_FORCE_HTTPS;
  }

  if (enable_static_pins_ && result.has_pins) {
    if (result.pinset_id >= g_hsts_source->pinsets_count)
      return false;

    pkp_result->domain = host.substr(result.hostname_offset);
    pkp_result->include_subdomains = result.pkp_include_subdomains;
    pkp_result->last_observed = base::GetBuildTime();

    const TransportSecurityStateSource::Pinset* pinset =
        &g_hsts_source->pinsets[result.pinset_id];
    if (pinset->report_uri != kNoReportURI)
      pkp_result->report_uri = GURL(pinset->report_uri);

    if (pinset->accepted_pins) {
      const char* const* sha256_hash = pinset->accepted_pins;
      while (*sha256_hash) {
        AddHash(*sha256_hash, &pkp_result->spki_hashes);
        sha256_hash++;
      }
    }
    if (pinset->rejected_pins) {
      const char* const* sha256_hash = pinset->rejected_pins;
      while (*sha256_hash) {
        AddHash(*sha256_hash, &pkp_result->bad_spki_hashes);
        sha256_hash++;
      }
    }
  }

  return true;
}

bool TransportSecurityState::GetSTSState(const std::string& host,
                                         STSState* result) {
  PKPState unused;
  return GetDynamicSTSState(host, result) ||
         GetStaticDomainState(host, result, &unused);
}

bool TransportSecurityState::GetPKPState(const std::string& host,
                                         PKPState* result) {
  STSState unused;
  return GetDynamicPKPState(host, result) ||
         GetStaticDomainState(host, &unused, result);
}

bool TransportSecurityState::GetDynamicSTSState(const std::string& host,
                                                STSState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    auto j = enabled_sts_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_sts_hosts_.end())
      continue;

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_sts_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // If this is the most specific STS match, add it to the result. Note: a STS
    // entry at a more specific domain overrides a less specific domain whether
    // or not |include_subdomains| is set.
    if (i == 0 || j->second.include_subdomains) {
      *result = j->second;
      result->domain = DNSDomainToString(host_sub_chunk);
      return true;
    }

    break;
  }

  return false;
}

bool TransportSecurityState::GetDynamicPKPState(const std::string& host,
                                                PKPState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    auto j = enabled_pkp_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_pkp_hosts_.end())
      continue;

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_pkp_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // If this is the most specific PKP match, add it to the result. Note: a PKP
    // entry at a more specific domain overrides a less specific domain whether
    // or not |include_subdomains| is set.
    if (i == 0 || j->second.include_subdomains) {
      *result = j->second;
      result->domain = DNSDomainToString(host_sub_chunk);
      return true;
    }

    break;
  }

  return false;
}

bool TransportSecurityState::GetDynamicExpectCTState(const std::string& host,
                                                     ExpectCTState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());
  auto j = enabled_expect_ct_hosts_.find(HashHost(canonicalized_host));
  if (j == enabled_expect_ct_hosts_.end())
    return false;
  // If the entry is invalid, drop it.
  if (current_time > j->second.expiry) {
    enabled_expect_ct_hosts_.erase(j);
    DirtyNotify();
    return false;
  }

  *result = j->second;
  return true;
}

void TransportSecurityState::AddOrUpdateEnabledSTSHosts(
    const std::string& hashed_host,
    const STSState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state.ShouldUpgradeToSSL());
  enabled_sts_hosts_[hashed_host] = state;
}

void TransportSecurityState::AddOrUpdateEnabledExpectCTHosts(
    const std::string& hashed_host,
    const ExpectCTState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state.enforce || !state.report_uri.is_empty());
  enabled_expect_ct_hosts_[hashed_host] = state;
}

TransportSecurityState::STSState::STSState()
    : upgrade_mode(MODE_DEFAULT), include_subdomains(false) {
}

TransportSecurityState::STSState::~STSState() = default;

bool TransportSecurityState::STSState::ShouldUpgradeToSSL() const {
  return upgrade_mode == MODE_FORCE_HTTPS;
}

TransportSecurityState::STSStateIterator::STSStateIterator(
    const TransportSecurityState& state)
    : iterator_(state.enabled_sts_hosts_.begin()),
      end_(state.enabled_sts_hosts_.end()) {
}

TransportSecurityState::STSStateIterator::~STSStateIterator() = default;

TransportSecurityState::PKPState::PKPState() : include_subdomains(false) {
}

TransportSecurityState::PKPState::PKPState(const PKPState& other) = default;

TransportSecurityState::PKPState::~PKPState() = default;

TransportSecurityState::ExpectCTState::ExpectCTState() : enforce(false) {}

TransportSecurityState::ExpectCTState::~ExpectCTState() = default;

TransportSecurityState::ExpectCTStateIterator::ExpectCTStateIterator(
    const TransportSecurityState& state)
    : iterator_(state.enabled_expect_ct_hosts_.begin()),
      end_(state.enabled_expect_ct_hosts_.end()) {
  state.AssertCalledOnValidThread();
}

TransportSecurityState::ExpectCTStateIterator::~ExpectCTStateIterator() =
    default;

bool TransportSecurityState::PKPState::CheckPublicKeyPins(
    const HashValueVector& hashes,
    std::string* failure_log) const {
  // Validate that hashes is not empty. By the time this code is called (in
  // production), that should never happen, but it's good to be defensive.
  // And, hashes *can* be empty in some test scenarios.
  if (hashes.empty()) {
    failure_log->append(
        "Rejecting empty public key chain for public-key-pinned domains: " +
        domain);
    return false;
  }

  if (HashesIntersect(bad_spki_hashes, hashes)) {
    failure_log->append("Rejecting public key chain for domain " + domain +
                        ". Validated chain: " + HashesToBase64String(hashes) +
                        ", matches one or more bad hashes: " +
                        HashesToBase64String(bad_spki_hashes));
    return false;
  }

  // If there are no pins, then any valid chain is acceptable.
  if (spki_hashes.empty())
    return true;

  if (HashesIntersect(spki_hashes, hashes)) {
    return true;
  }

  failure_log->append("Rejecting public key chain for domain " + domain +
                      ". Validated chain: " + HashesToBase64String(hashes) +
                      ", expected: " + HashesToBase64String(spki_hashes));
  return false;
}

bool TransportSecurityState::PKPState::HasPublicKeyPins() const {
  return spki_hashes.size() > 0 || bad_spki_hashes.size() > 0;
}

}  // namespace net
