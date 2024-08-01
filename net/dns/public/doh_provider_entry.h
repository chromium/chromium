// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_
#define NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/dns/public/dns_over_https_server_config.h"

namespace net {

// Represents insecure DNS, DoT, and DoH services run by the same provider.
// These entries are used to support upgrade from insecure DNS or DoT services
// to associated DoH services in automatic mode and to populate the dropdown
// menu for secure mode.
//
// To be eligible for auto-upgrade, an entry must have a non-empty
// `dns_over_53_server_ip_strs` or non-empty `dns_over_tls_hostnames`. To be
// eligible for the dropdown menu, the entry must have non-empty `ui_name` and
// `privacy_policy`. If `display_globally` is true, the entry is eligible to be
// displayed globally in the dropdown menu. If `display_globally` is false,
// `display_countries` should contain the two-letter ISO 3166-1 country codes,
// if any, where the entry is eligible for being displayed in the dropdown menu.
//
// If `feature` is disabled, the entry is eligible for neither auto-upgrade nor
// the dropdown menu.
struct NET_EXPORT DohProviderEntry {
 public:
  using List = std::vector<raw_ptr<const DohProviderEntry, VectorExperimental>>;

  enum class LoggingLevel {
    // Indicates the normal amount of logging, monitoring, and metrics.
    kNormal,

    // Indicates that a provider is of extra interest and eligible for
    // additional logging, monitoring, and metrics.
    kExtra,
  };

  std::string provider;
  // Avoid using base::Feature& and use raw_ref instead. Use feature.get()
  // for accessing the raw reference.
  raw_ref<const base::Feature> feature;
  std::set<IPAddress> ip_addresses;
  std::set<std::string> dns_over_tls_hostnames;
  DnsOverHttpsServerConfig doh_server_config;
  std::string ui_name;
  std::string privacy_policy;
  bool display_globally;
  std::set<std::string> display_countries;
  LoggingLevel logging_level;

  // Returns the full list of DoH providers. A subset of this list may be used
  // to support upgrade in automatic mode or to populate the dropdown menu for
  // secure mode.
  static const List& GetList();

  static DohProviderEntry ConstructForTesting(
      std::string provider,
      const base::Feature* feature,
      std::set<std::string_view> dns_over_53_server_ip_strs,
      std::set<std::string> dns_over_tls_hostnames,
      std::string dns_over_https_template,
      std::string ui_name,
      std::string privacy_policy,
      bool display_globally,
      std::set<std::string> display_countries,
      LoggingLevel logging_level = LoggingLevel::kNormal);

  // Entries are neither copyable nor moveable. This allows tests to construct a
  // List but ensures that `const DohProviderEntry*` is a safe type for
  // application code.
  DohProviderEntry(DohProviderEntry& other) = delete;
  DohProviderEntry(DohProviderEntry&& other) = delete;

  ~DohProviderEntry();

 private:
  DohProviderEntry(
      std::string provider,
      // Disallow implicit copying of the `feature` parameter because there
      // cannot be more than one `base::Feature` for a given feature name.
      const base::Feature* feature,
      std::set<std::string_view> dns_over_53_server_ip_strs,
      std::set<std::string> dns_over_tls_hostnames,
      std::string dns_over_https_template,
      std::string ui_name,
      std::string privacy_policy,
      bool display_globally,
      std::set<std::string> display_countries,
      LoggingLevel logging_level,
      std::set<std::string_view> dns_over_https_server_ip_strs = {});
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_
