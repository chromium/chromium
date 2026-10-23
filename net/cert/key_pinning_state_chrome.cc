// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_chrome.h"

#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "net/base/features.h"
#include "net/cert/key_pinning_state_chrome_data.h"
#include "net/http/transport_security_state_pins_source.h"

namespace net {
namespace {

struct DefaultOverrides {
  // RAW_PTR_EXCLUSION: pins_source always points to static data.
  RAW_PTR_EXCLUSION const TransportSecurityStatePinsSource* pins_source;
  bool enabled;
  bool pins_list_always_timely_for_testing;
};

std::optional<DefaultOverrides> g_default_state_overrides;

constexpr bool kDefaultEnableStaticPins =
// Static pinning is only enabled for official builds to make sure that
// others don't end up with pins that cannot be easily updated. The code is
// still compiled otherwise so that tests can be run without requiring to do an
// official build.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
    false;
#else
    true;
#endif

class KeyPinningStateChromeCompiledIn : public KeyPinningStateChrome {
 public:
  KeyPinningStateChromeCompiledIn();

 protected:
  base::Time GetPinListTimestamp() const override {
    return GetCompiledPinsListTimestamp();
  }

  bool GetPKPStateImpl(std::string_view normalized_host,
                       PKPState* pkp_result) const override;

 private:
  ~KeyPinningStateChromeCompiledIn() override;

  // RAW_PTR_EXCLUSION: static_pins_source_ always points to static data (or
  // null).
  RAW_PTR_EXCLUSION const TransportSecurityStatePinsSource* const
      static_pins_source_ = nullptr;
};

KeyPinningStateChromeCompiledIn::KeyPinningStateChromeCompiledIn()
    : static_pins_source_(g_default_state_overrides
                              ? g_default_state_overrides->pins_source
                              : GetCompiledPinsSource()) {}

KeyPinningStateChromeCompiledIn::~KeyPinningStateChromeCompiledIn() = default;

bool KeyPinningStateChromeCompiledIn::GetPKPStateImpl(
    std::string_view normalized_host,
    PKPState* pkp_result) const {
  std::string_view search_hostname = normalized_host;

  // TODO(crbug.com/497882860): this loop duplicates some (but not exactly)
  // logic from the KeyPinningStateChromeComponentUpdated::GetPKPStateImpl
  // operating on a different type. Think about if there is a better way to
  // structure this.
  while (true) {
    const TransportSecurityStatePinsSource::HostPin* pin =
        static_pins_source_->find_host_pin(search_hostname);
    // Only consider this a match if either include_subdomains is set, or
    // this is an exact match of the full hostname.
    if (pin &&
        (pin->include_subdomains || search_hostname == normalized_host)) {
      pkp_result->include_subdomains = pin->include_subdomains;
      for (const SHA256HashValue* hash : pin->pinset->accepted_pins) {
        pkp_result->spki_hashes.insert(*hash);
      }
      for (const SHA256HashValue* hash : pin->pinset->rejected_pins) {
        pkp_result->bad_spki_hashes.insert(*hash);
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
}

class KeyPinningStateChromeComponentUpdated : public KeyPinningStateChrome {
 public:
  KeyPinningStateChromeComponentUpdated(
      const std::vector<PinSet>& pinsets,
      const std::vector<PinSetInfo>& host_pins,
      base::Time update_time);

 protected:
  base::Time GetPinListTimestamp() const override {
    return key_pins_list_last_update_time_;
  }

  bool GetPKPStateImpl(std::string_view normalized_host,
                       PKPState* pkp_result) const override;

 private:
  using HostPinsMap =
      base::flat_map<std::string, std::pair<const PinSet*, bool>>;

  ~KeyPinningStateChromeComponentUpdated() override;

  // Creates the value for initializing `host_pins_`. Must be called after
  // `pinsets_` has been initialized.
  HostPinsMap::container_type InitializeHostPins(
      const std::vector<PinSetInfo>& host_pins) const;

  // These 3 members are for the component-updated "static" PKP data:
  // The values in host_pins_ maps are references to PinSet objects in the
  // pinsets_ vector.
  const base::Time key_pins_list_last_update_time_;
  const std::vector<PinSet> pinsets_;
  const HostPinsMap host_pins_;
};

KeyPinningStateChromeComponentUpdated::KeyPinningStateChromeComponentUpdated(
    const std::vector<PinSet>& pinsets,
    const std::vector<PinSetInfo>& host_pins,
    base::Time update_time)
    : key_pins_list_last_update_time_(update_time),
      pinsets_(pinsets),
      host_pins_(InitializeHostPins(host_pins)) {}

KeyPinningStateChromeComponentUpdated::
    ~KeyPinningStateChromeComponentUpdated() = default;

KeyPinningStateChromeComponentUpdated::HostPinsMap::container_type
KeyPinningStateChromeComponentUpdated::InitializeHostPins(
    const std::vector<PinSetInfo>& host_pins) const {
  auto pinset_names_map = base::MakeFlatMap<std::string_view, const PinSet*>(
      pinsets_, /*comp=*/{},
      [](const auto& pinset) -> std::pair<std::string_view, const PinSet*> {
        return {pinset.name(), &pinset};
      });

  // host_pins_ is populated once here and only read afterward via find(), so
  // flat_map (a sorted vector) is preferred over std::map for reduced
  // per-entry allocation overhead and better cache locality on lookups.
  // Duplicate hostnames do not occur in the component-delivered pin list.
  HostPinsMap::container_type entries;
  entries.reserve(host_pins.size());
  for (const auto& pin : host_pins) {
    auto it = pinset_names_map.find(pin.pinset_name_);
    if (it == pinset_names_map.end()) {
      // This should never happen, but if the component is bad and missing an
      // entry, we will ignore that particular pin.
      continue;
    }
    entries.emplace_back(pin.hostname_,
                         std::pair(it->second, pin.include_subdomains_));
  }
  return entries;
}

bool KeyPinningStateChromeComponentUpdated::GetPKPStateImpl(
    std::string_view normalized_host,
    PKPState* pkp_result) const {
  std::string_view search_hostname = normalized_host;

  // Use component updater supplied PKP data if available:
  while (true) {
    auto iter = host_pins_.find(search_hostname);
    // Only consider this a match if either include_subdomains is set, or
    // this is an exact match of the full hostname.
    if (iter != host_pins_.end() &&
        (iter->second.second || search_hostname == normalized_host)) {
      pkp_result->include_subdomains = iter->second.second;
      const PinSet* pinset = iter->second.first;
      for (const auto& hash : pinset->static_spki_hashes()) {
        pkp_result->spki_hashes.insert(hash);
      }
      for (const auto& hash : pinset->bad_static_spki_hashes()) {
        pkp_result->bad_spki_hashes.insert(hash);
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
}

}  // namespace

void SetKeyPinningStateChromeOverridesForTesting(
    const TransportSecurityStatePinsSource* source,
    bool enabled,
    bool pins_list_always_timely_for_testing) {
  g_default_state_overrides =
      DefaultOverrides{.pins_source = source ? source : GetCompiledPinsSource(),
                       .enabled = enabled,
                       .pins_list_always_timely_for_testing =
                           pins_list_always_timely_for_testing};
}

void ClearKeyPinningStateChromeOverridesForTesting() {
  g_default_state_overrides = std::nullopt;
}

KeyPinningStateChrome::PinSet::PinSet(
    std::string name,
    std::vector<SHA256HashValue> static_spki_hashes,
    std::vector<SHA256HashValue> bad_static_spki_hashes)
    : name_(std::move(name)),
      static_spki_hashes_(std::move(static_spki_hashes)),
      bad_static_spki_hashes_(std::move(bad_static_spki_hashes)) {}

KeyPinningStateChrome::PinSet::PinSet(const PinSet& other) = default;
KeyPinningStateChrome::PinSet::~PinSet() = default;

KeyPinningStateChrome::PinSetInfo::PinSetInfo(std::string hostname,
                                              std::string pinset_name,
                                              bool include_subdomains)
    : hostname_(std::move(hostname)),
      pinset_name_(std::move(pinset_name)),
      include_subdomains_(include_subdomains) {}

// static
scoped_refptr<KeyPinningStateChrome>
KeyPinningStateChrome::CreateForCompiledIn() {
  return base::MakeRefCounted<KeyPinningStateChromeCompiledIn>();
}

// static
scoped_refptr<KeyPinningStateChrome> KeyPinningStateChrome::CreateForUpdated(
    const std::vector<PinSet>& pinsets,
    const std::vector<PinSetInfo>& host_pins,
    base::Time update_time) {
  return base::MakeRefCounted<KeyPinningStateChromeComponentUpdated>(
      pinsets, host_pins, update_time);
}

// static
bool KeyPinningStateChrome::InitDefaultEnableState() {
  return g_default_state_overrides ? g_default_state_overrides->enabled
                                   : kDefaultEnableStaticPins;
}

// static
bool KeyPinningStateChrome::InitDefaultPinsListAlwaysTimelyState() {
  return g_default_state_overrides
             ? g_default_state_overrides->pins_list_always_timely_for_testing
             : false;
}

KeyPinningStateChrome::~KeyPinningStateChrome() = default;

bool KeyPinningStateChrome::GetPKPState(std::string_view host,
                                        PKPState* pkp_result) const {
  if (!enable_static_pins_ || !IsStaticPKPListTimely() ||
      !base::FeatureList::IsEnabled(features::kStaticKeyPinningEnforcement)) {
    return false;
  }

  // Ensure that |host| is a valid hostname before processing.
  if (CanonicalizeHost(host).empty()) {
    return false;
  }
  // Normalize any trailing '.' used for DNS suffix searches.
  size_t trailing_dot_found = host.find_last_not_of('.');
  if (trailing_dot_found == std::string::npos) {
    // Hostname is either empty or all dots
    return false;
  }
  host.remove_suffix(host.size() - trailing_dot_found - 1);
  std::string normalized_host = base::ToLowerASCII(host);

  return GetPKPStateImpl(normalized_host, pkp_result);
}

bool KeyPinningStateChrome::IsStaticPKPListTimely() const {
  if (pins_list_always_timely_for_testing_) {
    return true;
  }
  return (base::Time::Now() - GetPinListTimestamp()).InDays() <
         70 /* 10 weeks */;
}

// static
base::Time KeyPinningStateChrome::GetBuiltInPinsListTimestamp() {
  return GetCompiledPinsListTimestamp();
}

}  // namespace net
