// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
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
#include "net/cert/x509_certificate.h"
#include "net/dns/dns_names_util.h"
#include "net/extras/preload_data/decoder.h"
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

// Parameters for remembering sent HPKP reports.
const size_t kMaxReportCacheEntries = 50;
const int kTimeToRememberReportsMins = 60;
const size_t kReportCacheKeyLength = 16;

// Override for CheckCTRequirements() for unit tests. Possible values:
//   false: Use the default implementation (e.g. production)
//   true: Unless a delegate says otherwise, require CT.
bool g_ct_required_for_testing = false;

base::Value GetPEMEncodedChainAsList(const net::X509Certificate* cert_chain) {
  if (!cert_chain)
    return base::Value(base::Value::Type::LIST);

  base::Value::List result;
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  for (const std::string& cert : pem_encoded_chain)
    result.Append(cert);

  return base::Value(std::move(result));
}

bool HashReportForCache(const base::Value::Dict& report,
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

  base::Value::Dict report;
  base::Time now = base::Time::Now();
  report.Set("hostname", host_port_pair.host());
  report.Set("port", host_port_pair.port());
  report.Set("include-subdomains", pkp_state.include_subdomains);
  report.Set("noted-hostname", pkp_state.domain);

  auto served_certificate_chain_list =
      GetPEMEncodedChainAsList(served_certificate_chain);
  auto validated_certificate_chain_list =
      GetPEMEncodedChainAsList(validated_certificate_chain);
  report.Set("served-certificate-chain",
             std::move(served_certificate_chain_list));
  report.Set("validated-certificate-chain",
             std::move(validated_certificate_chain_list));

  base::Value::List known_pin_list;
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

  report.Set("known-pins", std::move(known_pin_list));

  // For the sent reports cache, do not include the effective expiration
  // date. The expiration date will likely change every time the user
  // visits the site, so it would prevent reports from being effectively
  // deduplicated.
  if (!HashReportForCache(report, pkp_state.report_uri, cache_key)) {
    LOG(ERROR) << "Failed to compute cache key for HPKP violation report.";
    return false;
  }

  report.Set("date-time", base::TimeToISO8601(now));
  report.Set("effective-expiration-date",
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

TransportSecurityState::HashedHost HashHost(
    base::span<const uint8_t> canonicalized_host) {
  return crypto::SHA256Hash(canonicalized_host);
}

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
bool HashesIntersect(const HashValueVector& a, const HashValueVector& b) {
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
std::vector<uint8_t> CanonicalizeHost(const std::string& host) {
  // We cannot perform the operations as detailed in the spec here as `host`
  // has already undergone IDN processing before it reached us. Thus, we
  // lowercase the input (probably redudnant since most input here has been
  // lowercased through URL canonicalization) and check that there are no
  // invalid characters in the host (via DNSDomainFromDot()).
  std::string lowered_host = base::ToLowerASCII(host);

  absl::optional<std::vector<uint8_t>> new_host =
      dns_names_util::DottedNameToNetwork(
          lowered_host,
          /*require_valid_internet_hostname=*/true);
  if (!new_host.has_value()) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::vector<uint8_t>();
  }

  return new_host.value();
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
BASE_FEATURE(kCertificateTransparencyEnforcement,
             "CertificateTransparencyEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

void SetTransportSecurityStateSourceForTesting(
    const TransportSecurityStateSource* source) {
  g_hsts_source = source ? source : kDefaultHSTSSource;
}

TransportSecurityState::TransportSecurityState()
    : TransportSecurityState(std::vector<std::string>()) {}

TransportSecurityState::TransportSecurityState(
    std::vector<std::string> hsts_host_bypass_list)
    : sent_hpkp_reports_cache_(kMaxReportCacheEntries) {
// Static pinning is only enabled for official builds to make sure that
// others don't end up with pins that cannot be easily updated.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_IOS)
  enable_static_pins_ = false;
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
  return GetSTSState(host, &unused_sts) || GetPKPState(host, &unused_pkp);
}

base::Value TransportSecurityState::NetLogUpgradeToSSLParam(
    const std::string& host) {
  STSState sts_state;
  base::Value::Dict dict;
  dict.Set("host", host);
  dict.Set("get_sts_state_result", GetSTSState(host, &sts_state));
  dict.Set("should_upgrade_to_ssl", sts_state.ShouldUpgradeToSSL());
  dict.Set("host_found_in_hsts_bypass_list",
           hsts_host_bypass_list_.find(host) != hsts_host_bypass_list_.end());
  return base::Value(std::move(dict));
}

bool TransportSecurityState::ShouldUpgradeToSSL(
    const std::string& host,
    const NetLogWithSource& net_log) {
  STSState sts_state;
  net_log.AddEvent(
      NetLogEventType::TRANSPORT_SECURITY_STATE_SHOULD_UPGRADE_TO_SSL,
      [&] { return NetLogUpgradeToSSLParam(host); });
  return GetSTSState(host, &sts_state) && sts_state.ShouldUpgradeToSSL();
}

TransportSecurityState::PKPStatus TransportSecurityState::CheckPublicKeyPins(
    const HostPortPair& host_port_pair,
    bool is_issued_by_known_root,
    const HashValueVector& public_key_hashes,
    const X509Certificate* served_certificate_chain,
    const X509Certificate* validated_certificate_chain,
    const PublicKeyPinReportStatus report_status,
    const NetworkAnonymizationKey& network_anonymization_key,
    std::string* pinning_failure_log) {
  // Perform pin validation only if the server actually has public key pins.
  if (!HasPublicKeyPins(host_port_pair.host())) {
    return PKPStatus::OK;
  }

  PKPStatus pin_validity = CheckPublicKeyPinsImpl(
      host_port_pair, is_issued_by_known_root, public_key_hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      network_anonymization_key, pinning_failure_log);

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
    ct::CTPolicyCompliance policy_compliance) {
  using CTRequirementLevel = RequireCTDelegate::CTRequirementLevel;
  std::string hostname = host_port_pair.host();

  // If CT is emergency disabled, either through a component updater set flag or
  // through the feature flag, we don't require CT for any host.
  if (ct_emergency_disable_ ||
      !base::FeatureList::IsEnabled(kCertificateTransparencyEnforcement)) {
    return CT_NOT_REQUIRED;
  }

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

  CTRequirementLevel ct_required = CTRequirementLevel::DEFAULT;
  if (require_ct_delegate_) {
    // Allow the delegate to override the CT requirement state.
    ct_required = require_ct_delegate_->IsCTRequiredForHost(
        hostname, validated_certificate_chain, public_key_hashes);
  }
  switch (ct_required) {
    case CTRequirementLevel::REQUIRED:
      return complies ? CT_REQUIREMENTS_MET : CT_REQUIREMENTS_NOT_MET;
    case CTRequirementLevel::NOT_REQUIRED:
      return CT_NOT_REQUIRED;
    case CTRequirementLevel::DEFAULT:
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

void TransportSecurityState::SetRequireCTDelegate(RequireCTDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  require_ct_delegate_ = delegate;
}

void TransportSecurityState::UpdatePinList(
    const std::vector<PinSet>& pinsets,
    const std::vector<PinSetInfo>& host_pins,
    base::Time update_time) {
  pinsets_ = pinsets;
  key_pins_list_last_update_time_ = update_time;
  host_pins_.emplace();
  std::map<std::string, PinSet const*> pinset_names_map;
  for (const auto& pinset : pinsets_) {
    pinset_names_map[pinset.name()] = &pinset;
  }
  for (const auto& pin : host_pins) {
    if (!base::Contains(pinset_names_map, pin.pinset_name_)) {
      // This should never happen, but if the component is bad and missing an
      // entry, we will ignore that particular pin.
      continue;
    }
    host_pins_.value()[pin.hostname_] = std::make_pair(
        pinset_names_map[pin.pinset_name_], pin.include_subdomains_);
  }
}

void TransportSecurityState::AddHSTSInternal(
    const std::string& host,
    TransportSecurityState::STSState::UpgradeMode upgrade_mode,
    const base::Time& expiry,
    bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
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
    const HashedHost hashed_host = HashHost(canonicalized_host);
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
  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
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
    const HashedHost hashed_host = HashHost(canonicalized_host);
    enabled_pkp_hosts_.erase(hashed_host);
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
    const net::NetworkAnonymizationKey& network_anonymization_key,
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
      base::TimeTicks::Now() + base::Minutes(kTimeToRememberReportsMins));

  report_sender_->Send(pkp_state.report_uri, "application/json; charset=utf-8",
                       serialized_report, network_anonymization_key,
                       base::OnceCallback<void()>(),
                       base::OnceCallback<void(const GURL&, int, int)>());
  return PKPStatus::VIOLATED;
}

bool TransportSecurityState::DeleteDynamicDataForHost(const std::string& host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  const HashedHost hashed_host = HashHost(canonicalized_host);
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

  if (deleted)
    DirtyNotify();
  return deleted;
}

void TransportSecurityState::ClearDynamicData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  enabled_sts_hosts_.clear();
  enabled_pkp_hosts_.clear();
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

// static
void TransportSecurityState::SetRequireCTForTesting(bool required) {
  g_ct_required_for_testing = required;
}

void TransportSecurityState::ClearReportCachesForTesting() {
  sent_hpkp_reports_cache_.Clear();
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
    const NetworkAnonymizationKey& network_anonymization_key,
    std::string* failure_log) {
  PKPState pkp_state;
  bool found_state = GetPKPState(host_port_pair.host(), &pkp_state);

  // HasPublicKeyPins should have returned true in order for this method to have
  // been called.
  DCHECK(found_state);
  return CheckPinsAndMaybeSendReport(
      host_port_pair, is_issued_by_known_root, pkp_state, hashes,
      served_certificate_chain, validated_certificate_chain, report_status,
      network_anonymization_key, failure_log);
}

bool TransportSecurityState::GetStaticSTSState(const std::string& host,
                                               STSState* sts_result) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsBuildTimely())
    return false;

  PreloadResult result;
  if (DecodeHSTSPreload(host, &result) &&
      hsts_host_bypass_list_.find(host) == hsts_host_bypass_list_.end() &&
      result.force_https) {
    sts_result->domain = host.substr(result.hostname_offset);
    sts_result->include_subdomains = result.sts_include_subdomains;
    sts_result->last_observed = base::GetBuildTime();
    sts_result->upgrade_mode = STSState::MODE_FORCE_HTTPS;
    return true;
  }

  return false;
}

bool TransportSecurityState::GetStaticPKPState(const std::string& host,
                                               PKPState* pkp_result) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!enable_static_pins_ || !IsStaticPKPListTimely() ||
      !base::FeatureList::IsEnabled(features::kStaticKeyPinningEnforcement)) {
    return false;
  }

  PreloadResult result;
  if (host_pins_.has_value()) {
    // Ensure that |host| is a valid hostname before processing.
    if (CanonicalizeHost(host).empty()) {
      return false;
    }
    // Normalize any trailing '.' used for DNS suffix searches.
    std::string normalized_host = host;
    size_t trailing_dot_found = normalized_host.find_last_not_of('.');
    if (trailing_dot_found == std::string::npos) {
      // Hostname is either empty or all dots
      return false;
    }
    normalized_host.erase(trailing_dot_found + 1);
    normalized_host = base::ToLowerASCII(normalized_host);

    base::StringPiece search_hostname = normalized_host;
    while (true) {
      auto iter = host_pins_->find(search_hostname);
      // Only consider this a match if either include_subdomains is set, or
      // this is an exact match of the full hostname.
      if (iter != host_pins_->end() &&
          (iter->second.second || search_hostname == normalized_host)) {
        pkp_result->domain = std::string(search_hostname);
        pkp_result->last_observed = key_pins_list_last_update_time_;
        pkp_result->include_subdomains = iter->second.second;
        const PinSet* pinset = iter->second.first;
        if (!pinset->report_uri().empty()) {
          pkp_result->report_uri = GURL(pinset->report_uri());
        }
        for (auto hash : pinset->static_spki_hashes()) {
          // If the update is malformed, it's preferable to skip the hash than
          // crash.
          if (hash.size() == 32) {
            AddHash(reinterpret_cast<const char*>(hash.data()),
                    &pkp_result->spki_hashes);
          }
        }
        for (auto hash : pinset->bad_static_spki_hashes()) {
          // If the update is malformed, it's preferable to skip the hash than
          // crash.
          if (hash.size() == 32) {
            AddHash(reinterpret_cast<const char*>(hash.data()),
                    &pkp_result->bad_spki_hashes);
          }
        }
        return true;
      }
      auto dot_pos = search_hostname.find(".");
      if (dot_pos == std::string::npos) {
        // If this was not a match, and there are no more dots in the string,
        // there are no more domains to try.
        return false;
      }
      // Try again in case this is a subdomain of a pinned domain that includes
      // subdomains.
      search_hostname = search_hostname.substr(dot_pos + 1);
    }
  } else if (DecodeHSTSPreload(host, &result) && result.has_pins) {
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
    return true;
  }

  return false;
}

bool TransportSecurityState::GetSTSState(const std::string& host,
                                         STSState* result) {
  return GetDynamicSTSState(host, result) || GetStaticSTSState(host, result);
}

bool TransportSecurityState::GetPKPState(const std::string& host,
                                         PKPState* result) {
  return GetDynamicPKPState(host, result) || GetStaticPKPState(host, result);
}

bool TransportSecurityState::GetDynamicSTSState(const std::string& host,
                                                STSState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    base::span<const uint8_t> host_sub_chunk =
        base::make_span(canonicalized_host).subspan(i);
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
          dns_names_util::NetworkToDottedName(host_sub_chunk);
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

  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    base::span<const uint8_t> host_sub_chunk =
        base::make_span(canonicalized_host).subspan(i);
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
          dns_names_util::NetworkToDottedName(host_sub_chunk);
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

void TransportSecurityState::AddOrUpdateEnabledSTSHosts(
    const HashedHost& hashed_host,
    const STSState& state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state.ShouldUpgradeToSSL());
  enabled_sts_hosts_[hashed_host] = state;
}

TransportSecurityState::STSState::STSState() = default;

TransportSecurityState::STSState::~STSState() = default;

bool TransportSecurityState::STSState::ShouldUpgradeToSSL() const {
  return upgrade_mode == MODE_FORCE_HTTPS;
}

TransportSecurityState::STSStateIterator::STSStateIterator(
    const TransportSecurityState& state)
    : iterator_(state.enabled_sts_hosts_.begin()),
      end_(state.enabled_sts_hosts_.end()) {}

TransportSecurityState::STSStateIterator::~STSStateIterator() = default;

TransportSecurityState::PKPState::PKPState() = default;

TransportSecurityState::PKPState::PKPState(const PKPState& other) = default;

TransportSecurityState::PKPState::~PKPState() = default;

TransportSecurityState::PinSet::PinSet(
    std::string name,
    std::vector<std::vector<uint8_t>> static_spki_hashes,
    std::vector<std::vector<uint8_t>> bad_static_spki_hashes,
    std::string report_uri)
    : name_(std::move(name)),
      static_spki_hashes_(std::move(static_spki_hashes)),
      bad_static_spki_hashes_(std::move(bad_static_spki_hashes)),
      report_uri_(std::move(report_uri)) {}

TransportSecurityState::PinSet::PinSet(const PinSet& other) = default;
TransportSecurityState::PinSet::~PinSet() = default;

TransportSecurityState::PinSetInfo::PinSetInfo(std::string hostname,
                                               std::string pinset_name,
                                               bool include_subdomains)
    : hostname_(std::move(hostname)),
      pinset_name_(std::move(pinset_name)),
      include_subdomains_(std::move(include_subdomains)) {}

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

bool TransportSecurityState::IsStaticPKPListTimely() const {
  if (pins_list_always_timely_for_testing_) {
    return true;
  }

  // If the list has not been updated via component updater, freshness depends
  // on the compiled-in list freshness.
  if (!host_pins_.has_value()) {
#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
    return (base::Time::Now() - kPinsListTimestamp).InDays() < 70;
#else
    return false;
#endif
  }
  DCHECK(!key_pins_list_last_update_time_.is_null());
  // Else, we use the last update time.
  return (base::Time::Now() - key_pins_list_last_update_time_).InDays() <
         70 /* 10 weeks */;
}

}  // namespace net
