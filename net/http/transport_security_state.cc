// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_state.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/hash.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/base/url_util.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/key_pinning_state.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/dns_names_util.h"
#include "net/extras/preload_data/decoder.h"
#include "net/http/http_security_headers.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"

#if BUILDFLAG(CRONET_BUILD)
#include "net/cert/key_pinning_state_dynamic.h"
#elif BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
#include "net/cert/key_pinning_state_chrome.h"
#endif

namespace net {

namespace {

#if BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
#include "net/http/transport_security_state_static.h"  // nogncheck
// Points to the active transport security state source.
const TransportSecurityStateSource* const kDefaultHSTSSource = &kHSTSSource;
#else
const TransportSecurityStateSource* const kDefaultHSTSSource = nullptr;
#endif

const TransportSecurityStateSource* g_hsts_source = kDefaultHSTSSource;

TransportSecurityState::HashedHost HashHost(
    base::span<const uint8_t> canonicalized_host) {
  return crypto::hash::Sha256(canonicalized_host);
}

// Converts |hostname| from dotted form ("www.google.com") to the form
// used in DNS: "\x03www\x06google\x03com", lowercases that, and returns
// the result.
std::vector<uint8_t> CanonicalizeHost(std::string_view host) {
  // We cannot perform the operations as detailed in the spec here as `host`
  // has already undergone IDN processing before it reached us. Thus, we
  // lowercase the input (probably redudnant since most input here has been
  // lowercased through URL canonicalization) and check that there are no
  // invalid characters in the host (via DNSDomainFromDot()).
  std::string lowered_host = base::ToLowerASCII(host);

  std::optional<std::vector<uint8_t>> new_host =
      dns_names_util::DottedNameToNetwork(
          lowered_host,
          /*require_valid_internet_hostname=*/true);
  if (!new_host.has_value()) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::vector<uint8_t>();
  }

  return std::move(new_host).value();
}

// PreloadResult is the result of resolving a specific name in the preloaded
// data.
struct PreloadResult {
  // hostname_offset contains the number of bytes from the start of the given
  // hostname where the name of the matching entry starts.
  size_t hostname_offset = 0;
  bool sts_include_subdomains = false;
  bool force_https = false;
};

using extras::PreloadDecoder;

// Extracts the current PreloadResult entry from the given Huffman encoded trie.
// If an "end of string" matches a period in the hostname then the information
// is remembered because, if no more specific node is found, then that
// information applies to the hostname.
class HSTSPreloadDecoder : public extras::PreloadDecoder {
 public:
  using extras::PreloadDecoder::PreloadDecoder;

  // extras::PreloadDecoder:
  bool ReadEntry(extras::PreloadDecoder::BitReader* reader,
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
          !reader->Next(&tmp.force_https)) {
        return false;
      }
    }

    tmp.hostname_offset = current_search_offset;

    if (current_search_offset == 0 ||
        search[current_search_offset - 1] == '.') {
      *out_found = tmp.sts_include_subdomains;

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

bool DecodeHSTSPreload(std::string_view search_hostname, PreloadResult* out) {
#if !BUILDFLAG(INCLUDE_TRANSPORT_SECURITY_STATE_PRELOAD_LIST)
  if (g_hsts_source == nullptr) {
    return false;
  }
#endif
  bool found = false;

  // Ensure that |search_hostname| is a valid hostname before
  // processing.
  if (CanonicalizeHost(search_hostname).empty()) {
    return false;
  }
  // Normalize any trailing '.' used for DNS suffix searches.
  size_t trailing_dot_found = search_hostname.find_last_not_of('.');
  if (trailing_dot_found == std::string::npos) {
    return false;
  }
  search_hostname.remove_suffix(search_hostname.size() - trailing_dot_found -
                                1);

  // |hostname| has already undergone IDN conversion, so should be
  // entirely A-Labels. The preload data is entirely normalized to
  // lower case.
  std::string hostname = base::ToLowerASCII(search_hostname);
  if (hostname.empty()) {
    return false;
  }

  HSTSPreloadDecoder decoder(
      g_hsts_source->huffman_tree, g_hsts_source->preloaded_data,
      g_hsts_source->preloaded_bits, g_hsts_source->root_position);
  if (!decoder.Decode(hostname, &found)) {
    DCHECK(false) << "Internal error in DecodeHSTSPreload for hostname "
                  << hostname;
    return false;
  }
  if (found) {
    *out = decoder.result();
  }
  return found;
}

}  // namespace

void SetTransportSecurityStateSourceForTesting(
    const TransportSecurityStateSource* source) {
  g_hsts_source = source ? source : kDefaultHSTSSource;
}

TransportSecurityState::TransportSecurityState()
    : TransportSecurityState(std::vector<std::string>()) {}

TransportSecurityState::TransportSecurityState(
    std::vector<std::string> hsts_host_bypass_list) {
  // Check that there no invalid entries in the static HSTS bypass list.
  for (auto& host : hsts_host_bypass_list) {
    DCHECK(host.find('.') == std::string::npos);
    hsts_host_bypass_list_.insert(host);
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// Both HSTS and HPKP cause fatal SSL errors, so return true if a
// host has either.
bool TransportSecurityState::ShouldSSLErrorsBeFatal(std::string_view host) {
  STSState unused_sts;
  return GetSTSState(host, &unused_sts) ||
         key_pinning_state_->HasPublicKeyPins(host);
}

base::DictValue TransportSecurityState::NetLogUpgradeToSSLParam(
    std::string_view host) {
  STSState sts_state;
  base::DictValue dict;
  dict.Set("host", host);
  dict.Set("get_sts_state_result", GetSTSState(host, &sts_state));
  dict.Set("should_upgrade_to_ssl", sts_state.ShouldUpgradeToSSL());
  dict.Set("host_found_in_hsts_bypass_list",
           hsts_host_bypass_list_.contains(host));
  return dict;
}

SSLUpgradeDecision TransportSecurityState::GetSSLUpgradeDecision(
    std::string_view host,
    bool is_top_level_nav,
    const NetLogWithSource& net_log) {
  net_log.AddEvent(
      NetLogEventType::TRANSPORT_SECURITY_STATE_SHOULD_UPGRADE_TO_SSL,
      [&] { return NetLogUpgradeToSSLParam(host); });

  // Only top level navigations should be upgraded when
  // kHstsTopLevelNavigationsOnly is enabled.
  if (!is_top_level_nav &&
      base::FeatureList::IsEnabled(features::kHstsTopLevelNavigationsOnly)) {
    return SSLUpgradeDecision::kNoUpgrade;
  }

  STSState sts_state;
  // Check the dynamic list first (removing the entry if expired).
  if (GetDynamicSTSState(host, &sts_state)) {
    // [*.]localhost hosts now ignore Strict-Transport-Security response
    // headers, but an entry may have been stored before this restriction
    // was introduced (crbug.com/41251622).
    if (sts_state.ShouldUpgradeToSSL() &&
        !(net::IsLocalHostname(host) &&
          base::FeatureList::IsEnabled(features::kIgnoreHSTSForLocalhost))) {
      // If the static state also requires an upgrade, the dynamic state
      // didn't need to be used in the decision.
      STSState static_sts_state;
      if (GetStaticSTSState(host, &static_sts_state) &&
          static_sts_state.ShouldUpgradeToSSL()) {
        return SSLUpgradeDecision::kStaticUpgrade;
      }
      return SSLUpgradeDecision::kDynamicUpgrade;
    }
    return SSLUpgradeDecision::kNoUpgrade;
  }
  if (GetStaticSTSState(host, &sts_state) && sts_state.ShouldUpgradeToSSL()) {
    return SSLUpgradeDecision::kStaticUpgrade;
  }
  return SSLUpgradeDecision::kNoUpgrade;
}

bool TransportSecurityState::ShouldUpgradeToSSL(
    std::string_view host,
    bool is_top_level_nav,
    const NetLogWithSource& net_log) {
  return GetSSLUpgradeDecision(host, is_top_level_nav, net_log) !=
         SSLUpgradeDecision::kNoUpgrade;
}

KeyPinningState::PKPStatus TransportSecurityState::CheckPublicKeyPins(
    std::string_view host,
    bool is_issued_by_known_root,
    const std::vector<SHA256HashValue>& public_key_hashes) const {
  return key_pinning_state_->CheckPublicKeyPins(host, is_issued_by_known_root,
                                                public_key_hashes);
}

bool TransportSecurityState::HasPublicKeyPins(std::string_view host) const {
  return key_pinning_state_->HasPublicKeyPins(host);
}

ct::CTRequirementsStatus TransportSecurityState::CheckCTRequirements(
    std::string_view host,
    bool is_issued_by_known_root,
    const std::vector<SHA256HashValue>& public_key_hashes,
    const X509Certificate* validated_certificate_chain,
    ct::CTPolicyCompliance policy_compliance) {
  // If CT is emergency disabled, we don't require CT for any host.
  if (ct_emergency_disable_) {
    return ct::CTRequirementsStatus::CT_NOT_REQUIRED;
  }

  // If CT is disabled, the delegate will be null. Don't require CT for any
  // host.
  if (!require_ct_delegate_) {
    return ct::CTRequirementsStatus::CT_NOT_REQUIRED;
  }

  return RequireCTDelegate::CheckCTRequirements(
      require_ct_delegate_.get(), host, is_issued_by_known_root,
      public_key_hashes, validated_certificate_chain, policy_compliance);
}

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate;
}

void TransportSecurityState::SetRequireCTDelegate(
    scoped_refptr<RequireCTDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  require_ct_delegate_ = std::move(delegate);
}

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
void TransportSecurityState::UpdatePinList(
    const std::vector<PinSet>& pinsets,
    const std::vector<PinSetInfo>& host_pins,
    base::Time update_time) {
  key_pinning_state_ =
      KeyPinningStateChrome::CreateForUpdated(pinsets, host_pins, update_time);
}
#endif

void TransportSecurityState::AddHSTSInternal(
    std::string_view host,
    TransportSecurityState::STSState::UpgradeMode upgrade_mode,
    base::Time expiry,
    bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty()) {
    return;
  }

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

#if BUILDFLAG(CRONET_BUILD)
void TransportSecurityState::AddHPKP(std::string_view host,
                                     base::Time expiry,
                                     bool include_subdomains,
                                     const HashValueVector& hashes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  key_pinning_state_->AddHPKP(host, expiry, include_subdomains, hashes);
}

void TransportSecurityState::
    SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value) {
  key_pinning_state_->SetEnablePublicKeyPinningBypassForLocalTrustAnchors(
      value);
}
#endif

bool TransportSecurityState::DeleteDynamicDataForHost(std::string_view host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty()) {
    return false;
  }

  const HashedHost hashed_host = HashHost(canonicalized_host);
  bool deleted = enabled_sts_hosts_.erase(hashed_host) > 0;

  if (deleted) {
    DirtyNotify();
  }
  return deleted;
}

void TransportSecurityState::ClearDynamicData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  enabled_sts_hosts_.clear();
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

  if (dirtied && delegate_) {
    delegate_->WriteNow(this, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

TransportSecurityState::~TransportSecurityState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TransportSecurityState::DirtyNotify() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (delegate_) {
    delegate_->StateIsDirty(this);
  }
}

bool TransportSecurityState::AddHSTSHeader(std::string_view host,
                                           std::string_view value) {
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

void TransportSecurityState::AddHSTS(std::string_view host,
                                     base::Time expiry,
                                     bool include_subdomains) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AddHSTSInternal(host, STSState::MODE_FORCE_HTTPS, expiry, include_subdomains);
}

size_t TransportSecurityState::num_sts_entries() const {
  return enabled_sts_hosts_.size();
}

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
// static
base::Time TransportSecurityState::GetBuiltInPinsListTimestamp() {
  return KeyPinningStateChrome::GetBuiltInPinsListTimestamp();
}
#endif

// static
bool TransportSecurityState::IsBuildTimely() {
  const base::Time build_time = base::GetBuildTime();
  // We consider built-in information to be timely for 10 weeks.
  return (base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */;
}

bool TransportSecurityState::GetStaticSTSState(std::string_view host,
                                               STSState* sts_result) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsBuildTimely()) {
    return false;
  }

  PreloadResult result;
  if (DecodeHSTSPreload(host, &result) &&
      !hsts_host_bypass_list_.contains(host) && result.force_https) {
    sts_result->domain = host.substr(result.hostname_offset);
    sts_result->include_subdomains = result.sts_include_subdomains;
    sts_result->last_observed = base::GetBuildTime();
    sts_result->upgrade_mode = STSState::MODE_FORCE_HTTPS;
    return true;
  }

  return false;
}

bool TransportSecurityState::GetSTSState(std::string_view host,
                                         STSState* result) {
  return GetDynamicSTSState(host, result) || GetStaticSTSState(host, result);
}

bool TransportSecurityState::GetPKPState(std::string_view host,
                                         PKPState* pkp_result) const {
  return key_pinning_state_->GetPKPState(host, pkp_result);
}

bool TransportSecurityState::GetDynamicSTSState(std::string_view host,
                                                STSState* result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const std::vector<uint8_t> canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty()) {
    return false;
  }

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    base::span<const uint8_t> host_sub_chunk =
        base::span(canonicalized_host).subspan(i);
    auto j = enabled_sts_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_sts_hosts_.end()) {
      continue;
    }

    // If the entry is invalid, drop it.
    if (current_time > j->second.expiry) {
      enabled_sts_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    // An entry matches if it is either an exact match, or if it is a prefix
    // match and the includeSubDomains directive was included.
    if (i == 0 || j->second.include_subdomains) {
      std::optional<std::string> dotted_name =
          dns_names_util::NetworkToDottedName(host_sub_chunk);
      if (!dotted_name) {
        return false;
      }

      *result = j->second;
      result->domain = std::move(dotted_name).value();
      return true;
    }
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

}  // namespace net
