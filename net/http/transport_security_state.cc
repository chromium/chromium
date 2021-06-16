// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/time/time_to_iso8601.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/symantec_certs.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/dns_util.h"
#include "net/extras/preload_data/decoder.h"
#include "net/http/hsts_info.h"
#include "net/http/http_security_headers.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

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
//   false: Use the default implementation (e.g. production)
//   true: Unless a delegate says otherwise, require CT.
bool g_ct_required_for_testing = false;

bool IsDynamicExpectCTEnabled() {
  return base::FeatureList::IsEnabled(
      TransportSecurityState::kDynamicExpectCTFeature);
}

base::Value GetPEMEncodedChainAsList(const net::X509Certificate* cert_chain) {
  if (!cert_chain)
    return base::Value(base::Value::Type::LIST);

  base::Value result(base::Value::Type::LIST);
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  for (const std::string& cert : pem_encoded_chain)
    result.Append(cert);

  return result;
}

bool HashReportForCache(const base::Value& report,
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

  base::Value report(base::Value::Type::DICTIONARY);
  base::Time now = base::Time::Now();
  report.SetStringKey("hostname", host_port_pair.host());
  report.SetIntKey("port", host_port_pair.port());
  report.SetBoolKey("include-subdomains", pkp_state.include_subdomains);
  report.SetStringKey("noted-hostname", pkp_state.domain);

  auto served_certificate_chain_list =
      GetPEMEncodedChainAsList(served_certificate_chain);
  auto validated_certificate_chain_list =
      GetPEMEncodedChainAsList(validated_certificate_chain);
  report.SetKey("served-certificate-chain",
                std::move(served_certificate_chain_list));
  report.SetKey("validated-certificate-chain",
                std::move(validated_certificate_chain_list));

  base::Value known_pin_list(base::Value::Type::LIST);
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

    known_pin_list.Append(known_pin);
  }

  report.SetKey("known-pins", std::move(known_pin_list));

  // For the sent reports cache, do not include the effective expiration
  // date. The expiration date will likely change every time the user
  // visits the site, so it would prevent reports from being effectively
  // deduplicated.
  if (!HashReportForCache(report, pkp_state.report_uri, cache_key)) {
    LOG(ERROR) << "Failed to compute cache key for HPKP violation report.";
    return false;
  }

  report.SetStringKey("date-time", base::TimeToISO8601(now));
  report.SetStringKey("effective-expiration-date",
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

std::string HashHost(base::StringPiece canonicalized_host) {
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
    : TransportSecurityState(std::vector<std::string>()) {
  // By default the CT log list is treated as last updated at build time (since
  // a compiled-in list is used), this is overridden if the list is dynamically
  // updated.
  ct_log_list_last_update_time_ = base::GetBuildTime();
}

TransportSecurityState::TransportSecurityState(
    std::vector<std::string> hsts_host_bypass_list)
    : enable_static_pins_(true),
      enable_static_expect_ct_(true),
      enable_pkp_bypass_for_local_trust_anchors_(true),
      sent_hpkp_reports_cache_(kMaxReportCacheEntries),
      sent_expect_ct_reports_cache_(kMaxReportCacheEntries),
      key_expect_ct_by_nik_(base::FeatureList::IsEnabled(
          features::kPartitionExpectCTStateByNetworkIsolationKey)) {
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
  return GetDynamicSTSState(host, &unused_sts) ||
         GetDynamicPKPState(host, &unused_pkp) ||
         GetStaticDomainState(host, &unused_sts, &unused_pkp);
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
    const NetworkIsolationKey& network_isolation_key,
    std::string* pinning_failure_log) {
  // Perform pin validation only if the server actually has public key pins.
  if (!HasPublicKeyPins(host_port_pair.host())) {
    return PKPStatus::OK;
  }

  PKPStatus pin_validity = CheckPublicKeyPinsImpl(
      host_port_pair, is_issued_by_known_root, public_key_hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      network_isolation_key, pinning_failure_log);

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
    ct::CTPolicyCompliance policy_compliance,
    const NetworkIsolationKey& network_isolation_key) {
  using CTRequirementLevel = RequireCTDelegate::CTRequirementLevel;
  std::string hostname = host_port_pair.host();

  // If CT emergency disable flag is set, we don't require CT for any host.
  if (ct_emergency_disable_)
    return CT_NOT_REQUIRED;

  // CT is not required if the certificate does not chain to a publicly
  // trusted root certificate. Testing can override this, as certain tests
  // rely on using a non-publicly-trusted root.
  if (!is_issued_by_known_root && !g_ct_required_for_testing)
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
  if (IsDynamicExpectCTEnabled() &&
      GetDynamicExpectCTState(hostname, network_isolation_key, &state)) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.ExpectCTHeader.PolicyComplianceOnConnectionSetup",
        policy_compliance, ct::CTPolicyCompliance::CT_POLICY_COUNT);
    if (!complies && expect_ct_reporter_ && !state.report_uri.is_empty() &&
        report_status == ENABLE_EXPECT_CT_REPORTS) {
      MaybeNotifyExpectCTFailed(
          host_port_pair, state.report_uri, state.expiry,
          validated_certificate_chain, served_certificate_chain,
          signed_certificate_timestamps, network_isolation_key);
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
  if (found || g_ct_required_for_testing)
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

void TransportSecurityState::SetCTLogListUpdateTime(base::Time update_time) {
  ct_log_list_last_update_time_ = update_time;
}

void TransportSecurityState::AddHSTSInternal(
    const std::string& host,
    TransportSecurityState::STSState::UpgradeMode upgrade_mode,
    const base::Time& expiry,
    bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  STSState sts_state;
  // No need to store |sts_state.domain| since it is redundant.
  // (|canonicalized_host| is the map key.)
  sts_state.last_observed = base::Time::Now();
  sts_state.include_subdomains = include_subdomains;
  sts_state.expiry = expiry;
  sts_state.upgrade_mode = upgrade_mode;

  // Only store new state when HSTS is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (sts_state.ShouldUpgradeToSSL()) {
    enabled_sts_hosts_[HashHost(canonicalized_host)] = sts_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_sts_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

void TransportSecurityState::AddHPKPInternal(const std::string& host,
                                             const base::Time& last_observed,
                                             const base::Time& expiry,
                                             bool include_subdomains,
                                             const HashValueVector& hashes,
                                             const GURL& report_uri) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  PKPState pkp_state;
  // No need to store |pkp_state.domain| since it is redundant.
  // (|canonicalized_host| is the map key.)
  pkp_state.last_observed = last_observed;
  pkp_state.expiry = expiry;
  pkp_state.include_subdomains = include_subdomains;
  pkp_state.spki_hashes = hashes;
  pkp_state.report_uri = report_uri;

  // Only store new state when HPKP is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  if (pkp_state.HasPublicKeyPins()) {
    enabled_pkp_hosts_[HashHost(canonicalized_host)] = pkp_state;
  } else {
    const std::string hashed_host = HashHost(canonicalized_host);
    enabled_pkp_hosts_.erase(hashed_host);
  }

  DirtyNotify();
}

void TransportSecurityState::AddExpectCTInternal(
    const std::string& host,
    const base::Time& last_observed,
    const base::Time& expiry,
    bool enforce,
    const GURL& report_uri,
    const NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!IsDynamicExpectCTEnabled())
    return;

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  ExpectCTState expect_ct_state;
  // No need to store |expect_ct_state.domain| since it is redundant.
  // (|canonicalized_host| is the map key.)
  expect_ct_state.last_observed = last_observed;
  expect_ct_state.expiry = expiry;
  expect_ct_state.enforce = enforce;
  expect_ct_state.report_uri = report_uri;

  // Only store new state when Expect-CT is explicitly enabled. If it is
  // disabled, remove the state from the enabled hosts.
  ExpectCTStateIndex index = CreateExpectCTStateIndex(
      HashHost(canonicalized_host), network_isolation_key);
  if (expect_ct_state.enforce || !expect_ct_state.report_uri.is_empty()) {
    enabled_expect_ct_hosts_[index] = expect_ct_state;
    MaybePruneExpectCTState();
  } else {
    enabled_expect_ct_hosts_.erase(index);
  }

  DirtyNotify();
}

void TransportSecurityState::
    SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value) {
  enable_pkp_bypass_for_local_trust_anchors_ = value;
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
    const net::NetworkIsolationKey& network_isolation_key,
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
                       serialized_report, network_isolation_key,
                       base::OnceCallback<void()>(),
                       base::OnceCallback<void(const GURL&, int, int)>());
  return PKPStatus::VIOLATED;
}

bool TransportSecurityState::GetStaticExpectCTState(
    const std::string& host,
    ExpectCTState* expect_ct_state) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsCTLogListTimely())
    return false;

  PreloadResult result;
  if (!DecodeHSTSPreload(host, &result))
    return false;

  if (!enable_static_expect_ct_ || !result.expect_ct)
    return false;

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
        signed_certificate_timestamps,
    const NetworkIsolationKey& network_isolation_key) {
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
      served_certificate_chain, signed_certificate_timestamps,
      network_isolation_key);
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

  // Delete matching entries for all NetworkIsolationKeys. Performance isn't
  // important here, since this is only called when directly initiated by the
  // user, so a linear search is fine.
  for (auto it = enabled_expect_ct_hosts_.begin();
       it != enabled_expect_ct_hosts_.end();) {
    auto current = it;
    ++it;
    if (current->first.hashed_host != hashed_host)
      continue;
    enabled_expect_ct_hosts_.erase(current);
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

void TransportSecurityState::DeleteAllDynamicDataBetween(
    base::Time start_time,
    base::Time end_time,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool dirtied = false;
  auto sts_iterator = enabled_sts_hosts_.begin();
  while (sts_iterator != enabled_sts_hosts_.end()) {
    if (sts_iterator->second.last_observed >= start_time &&
        sts_iterator->second.last_observed < end_time) {
      dirtied = true;
      enabled_sts_hosts_.erase(sts_iterator++);
      continue;
    }

    ++sts_iterator;
  }

  auto pkp_iterator = enabled_pkp_hosts_.begin();
  while (pkp_iterator != enabled_pkp_hosts_.end()) {
    if (pkp_iterator->second.last_observed >= start_time &&
        pkp_iterator->second.last_observed < end_time) {
      dirtied = true;
      enabled_pkp_hosts_.erase(pkp_iterator++);
      continue;
    }

    ++pkp_iterator;
  }

  auto expect_ct_iterator = enabled_expect_ct_hosts_.begin();
  while (expect_ct_iterator != enabled_expect_ct_hosts_.end()) {
    if (expect_ct_iterator->second.last_observed >= start_time &&

        expect_ct_iterator->second.last_observed < end_time) {
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

void TransportSecurityState::AddExpectCT(
    const std::string& host,
    const base::Time& expiry,
    bool enforce,
    const GURL& report_uri,
    const NetworkIsolationKey& network_isolation_key) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AddExpectCTInternal(host, base::Time::Now(), expiry, enforce, report_uri,
                      network_isolation_key);
}

void TransportSecurityState::ProcessExpectCTHeader(
    const std::string& value,
    const HostPortPair& host_port_pair,
    const SSLInfo& ssl_info,
    const NetworkIsolationKey& network_isolation_key) {
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
      MaybeNotifyExpectCTFailed(
          host_port_pair, state.report_uri, base::Time(), ssl_info.cert.get(),
          ssl_info.unverified_cert.get(),
          ssl_info.signed_certificate_timestamps, network_isolation_key);
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
        !GetDynamicExpectCTState(host_port_pair.host(), network_isolation_key,
                                 &state)) {
      MaybeNotifyExpectCTFailed(
          host_port_pair, report_uri, base::Time(), ssl_info.cert.get(),
          ssl_info.unverified_cert.get(),
          ssl_info.signed_certificate_timestamps, network_isolation_key);
    }
    return;
  }
  AddExpectCTInternal(host_port_pair.host(), now, now + max_age, enforce,
                      report_uri, network_isolation_key);
}

// static
void TransportSecurityState::SetRequireCTForTesting(bool required) {
  g_ct_required_for_testing = required;
}

void TransportSecurityState::ClearReportCachesForTesting() {
  sent_hpkp_reports_cache_.Clear();
  sent_expect_ct_reports_cache_.Clear();
}

size_t TransportSecurityState::num_expect_ct_entries() const {
  return enabled_expect_ct_hosts_.size();
}

size_t TransportSecurityState::num_sts_entries() const {
  return enabled_sts_hosts_.size();
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
    const NetworkIsolationKey& network_isolation_key,
    std::string* failure_log) {
  PKPState pkp_state;
  bool found_state = GetPKPState(host_port_pair.host(), &pkp_state);

  // HasPublicKeyPins should have returned true in order for this method to have
  // been called.
  DCHECK(found_state);
  return CheckPinsAndMaybeSendReport(
      host_port_pair, is_issued_by_known_root, pkp_state, hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      network_isolation_key, failure_log);
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
    base::StringPiece host_sub_chunk =
        base::StringPiece(canonicalized_host).substr(i);
    auto j = enabled_sts_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_sts_hosts_.end())
      continue;

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_sts_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // An entry matches if it is either an exact match, or if it is a prefix
    // match and the includeSubDomains directive was included.
    if (i == 0 || j->second.include_subdomains) {
      absl::optional<std::string> dotted_name =
          DnsDomainToString(host_sub_chunk);
      if (!dotted_name)
        return false;

      *result = j->second;
      result->domain = std::move(dotted_name).value();
      return true;
    }
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
    base::StringPiece host_sub_chunk =
        base::StringPiece(canonicalized_host).substr(i);
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
    //
    // TODO(davidben): This does not match the HSTS behavior. We no longer
    // implement HPKP, so this logic is only used via AddHPKP(), reachable from
    // Cronet.
    if (i == 0 || j->second.include_subdomains) {
      absl::optional<std::string> dotted_name =
          DnsDomainToString(host_sub_chunk);
      if (!dotted_name)
        return false;

      *result = j->second;
      result->domain = std::move(dotted_name).value();
      return true;
    }

    break;
  }

  return false;
}

bool TransportSecurityState::GetDynamicExpectCTState(
    const std::string& host,
    const NetworkIsolationKey& network_isolation_key,
    ExpectCTState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());
  auto j = enabled_expect_ct_hosts_.find(CreateExpectCTStateIndex(
      HashHost(canonicalized_host), network_isolation_key));
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
    const NetworkIsolationKey& network_isolation_key,
    const ExpectCTState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state.enforce || !state.report_uri.is_empty());
  enabled_expect_ct_hosts_[CreateExpectCTStateIndex(
      hashed_host, network_isolation_key)] = state;
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

TransportSecurityState::ExpectCTStateIndex::ExpectCTStateIndex(
    const std::string& hashed_host,
    const NetworkIsolationKey& network_isolation_key,
    bool respect_network_isolation_keyn_key)
    : hashed_host(hashed_host),
      network_isolation_key(respect_network_isolation_keyn_key
                                ? network_isolation_key
                                : NetworkIsolationKey()) {}

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

TransportSecurityState::ExpectCTStateIndex
TransportSecurityState::CreateExpectCTStateIndex(
    const std::string& hashed_host,
    const NetworkIsolationKey& network_isolation_key) {
  return ExpectCTStateIndex(hashed_host, network_isolation_key,
                            key_expect_ct_by_nik_);
}

void TransportSecurityState::MaybePruneExpectCTState() {
  if (!base::FeatureList::IsEnabled(features::kExpectCTPruning) ||
      enabled_expect_ct_hosts_.size() <
          static_cast<size_t>(features::kExpectCTPruneMax.Get())) {
    return;
  }

  base::Time now = base::Time::Now();
  if (now < earliest_next_prune_expect_ct_time_)
    return;

  earliest_next_prune_expect_ct_time_ =
      now +
      base::TimeDelta::FromSeconds(features::kExpectCTPruneDelaySecs.Get());

  base::Time last_prunable_observation_time =
      now -
      base::TimeDelta::FromDays(features::kExpectCTSafeFromPruneDays.Get());

  // Cache this locally, so don't have to repeatedly query the value.
  size_t expect_ct_prune_min = features::kExpectCTPruneMin.Get();

  // Entries that are eligible to be pruned based on the global (not per-NIK)
  // entry limit.
  std::vector<ExpectCTStateMap::iterator> prunable_expect_ct_entries;

  // Clear expired entries first. If that's enough, maybe no valid entries have
  // to be removed. Also populate |prunable_expect_ct_entries|.
  for (auto expect_ct_iterator = enabled_expect_ct_hosts_.begin();
       expect_ct_iterator != enabled_expect_ct_hosts_.end();) {
    if (expect_ct_iterator->second.expiry < now) {
      enabled_expect_ct_hosts_.erase(expect_ct_iterator++);
      continue;
    }

    // If there are fewer than |expect_ct_prune_min| entries remaining, no need
    // to delete anything else.
    if (enabled_expect_ct_hosts_.size() <= expect_ct_prune_min)
      return;

    // Entries that are older than the prunable time window, are report-only, or
    // have a transient NetworkIsolationKey, are considered prunable.
    //
    // If |key_expect_ct_by_nik_| is false, all entries have an empty NIK.
    // IsTransient() returns true for the empty NIK, despite entries being saved
    // to disk, so don't want to delete entries with empty NIKs.
    if (expect_ct_iterator->second.last_observed <
            last_prunable_observation_time ||
        !expect_ct_iterator->second.enforce ||
        (key_expect_ct_by_nik_ &&
         expect_ct_iterator->first.network_isolation_key.IsTransient())) {
      prunable_expect_ct_entries.push_back(expect_ct_iterator);
    }
    ++expect_ct_iterator;
  }

  // Number of entries that need to be removed to reach |expect_ct_prune_min|.
  size_t num_entries_to_prune =
      enabled_expect_ct_hosts_.size() - expect_ct_prune_min;
  if (num_entries_to_prune < prunable_expect_ct_entries.size()) {
    // There are more than enough prunable entries to reach kExpectCTPruneMin.
    // Find the |num_entries_to_prune| most prunable entries, according to
    // ExpectCTPruningSorter.
    auto expect_ct_prune_end =
        prunable_expect_ct_entries.begin() + num_entries_to_prune;
    std::partial_sort(prunable_expect_ct_entries.begin(), expect_ct_prune_end,
                      prunable_expect_ct_entries.end(), ExpectCTPruningSorter);
  } else {
    // Otherwise, delete all prunable entries.
    num_entries_to_prune = prunable_expect_ct_entries.size();
  }
  DCHECK_LE(num_entries_to_prune, prunable_expect_ct_entries.size());

  for (size_t i = 0; i < num_entries_to_prune; ++i) {
    enabled_expect_ct_hosts_.erase(prunable_expect_ct_entries[i]);
  }

  // If there are fewer than |kExpectCTPruneMin| entries remaining, or entries
  // are not being keyed by NetworkIsolationKey, nothing left to do.
  if (enabled_expect_ct_hosts_.size() <= expect_ct_prune_min ||
      !key_expect_ct_by_nik_) {
    return;
  }

  // Otherwise, cap the number of entries per NetworkIsolationKey to
  // |kMaxEntriesPerNik|.

  // Create a vector of all the ExpectCT entries for each NIK.
  std::map<net::NetworkIsolationKey, std::vector<ExpectCTStateMap::iterator>>
      nik_map;
  for (auto expect_ct_iterator = enabled_expect_ct_hosts_.begin();
       expect_ct_iterator != enabled_expect_ct_hosts_.end();
       ++expect_ct_iterator) {
    nik_map[expect_ct_iterator->first.network_isolation_key].push_back(
        expect_ct_iterator);
  }

  // For each NIK with more than the maximum number of entries, remove the most
  // prunable entries until it has exactly |kExpectCTMaxEntriesPerNik| entries.
  size_t max_entries_per_nik = features::kExpectCTMaxEntriesPerNik.Get();
  for (auto& nik_entries : nik_map) {
    if (nik_entries.second.size() < max_entries_per_nik)
      continue;
    auto top_frame_origin_prune_end = nik_entries.second.begin() +
                                      nik_entries.second.size() -
                                      max_entries_per_nik;
    std::partial_sort(nik_entries.second.begin(), top_frame_origin_prune_end,
                      nik_entries.second.end(), ExpectCTPruningSorter);
    for (auto entry_to_prune = nik_entries.second.begin();
         entry_to_prune != top_frame_origin_prune_end; ++entry_to_prune) {
      enabled_expect_ct_hosts_.erase(*entry_to_prune);
    }
  }
}

bool TransportSecurityState::ExpectCTPruningSorter(
    const ExpectCTStateMap::iterator& it1,
    const ExpectCTStateMap::iterator& it2) {
  // std::tie requires r-values, so have to put these on the stack to use
  // std::tie.
  bool is_not_transient1 = !it1->first.network_isolation_key.IsTransient();
  bool is_not_transient2 = !it2->first.network_isolation_key.IsTransient();
  return std::tie(is_not_transient1, it1->second.enforce,
                  it1->second.last_observed) <
         std::tie(is_not_transient2, it2->second.enforce,
                  it2->second.last_observed);
}

bool TransportSecurityState::IsCTLogListTimely() const {
  // Preloaded Expect-CT is enforced if the CT log list is timely. Note that
  // unlike HSTS and HPKP, the date of the preloaded list itself (i.e.
  // base::GetBuildTime()) is not directly consulted. Consulting the
  // build time would allow sites that have subsequently disabled Expect-CT
  // to opt-out. However, because as of June 2021, all unexpired certificates
  // are already expected to comply with the policies expressed by Expect-CT,
  // there's no need to offer an opt-out.
  return (base::Time::Now() - ct_log_list_last_update_time_).InDays() <
         70 /* 10 weeks */;
}

}  // namespace net
