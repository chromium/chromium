// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_
#define NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_

#include <set>
#include <string>
#include <vector>

#include "base/optional.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"

namespace net {

// Provider ids for usage in histograms. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "DohProviderId" in src/tools/metrics/histograms/enums.xml.
enum class DohProviderIdForHistogram {
  kCustom = 0,
  kCleanBrowsingFamily = 1,
  kCloudflare = 2,
  kGoogle = 3,
  kIij = 4,
  kQuad9Secure = 5,
  kDnsSb = 6,
  kCznic = 7,
  kNextDns = 8,
  kOpenDns = 9,
  kAlekBergNl = 10,
  kMaxValue = kAlekBergNl,
};

// Represents insecure DNS, DoT, and DoH services run by the same provider.
// These entries are used to support upgrade from insecure DNS or DoT services
// to associated DoH services in automatic mode and to populate the dropdown
// menu for secure mode. To be eligible for auto-upgrade, entries must have a
// non-empty |ip_strs| or non-empty |dns_over_tls_hostnames|. To be eligible for
// the dropdown menu, entries must have non-empty |ui_name| and
// |privacy_policy|. If |display_globally| is true, the entry is eligible for
// being displayed globally in the dropdown menu. If |display_globally| is
// false, |display_countries| should contain the two-letter ISO 3166-1 country
// codes, if any, where the entry is eligible for being displayed in the
// dropdown menu.
struct NET_EXPORT DohProviderEntry {
 public:
  using List = std::vector<const DohProviderEntry*>;

  std::string provider;
  // A provider_id_for_histogram is required for entries that are intended to
  // be visible in the UI.
  base::Optional<DohProviderIdForHistogram> provider_id_for_histogram;
  std::set<IPAddress> ip_addresses;
  std::set<std::string> dns_over_tls_hostnames;
  std::string dns_over_https_template;
  std::string ui_name;
  std::string privacy_policy;
  bool display_globally;
  std::set<std::string> display_countries;

  // Returns the full list of DoH providers. A subset of this list may be used
  // to support upgrade in automatic mode or to populate the dropdown menu for
  // secure mode.
  static const List& GetList();

  static DohProviderEntry ConstructForTesting(
      std::string provider,
      base::Optional<DohProviderIdForHistogram> provider_id_for_histogram,
      std::set<base::StringPiece> ip_strs,
      std::set<std::string> dns_over_tls_hostnames,
      std::string dns_over_https_template,
      std::string ui_name,
      std::string privacy_policy,
      bool display_globally,
      std::set<std::string> display_countries);

  // Entries are move-only.  This allows tests to construct a List but ensures
  // that |const DohProviderEntry*| is a safe type for application code.
  DohProviderEntry(DohProviderEntry&& other);
  DohProviderEntry& operator=(DohProviderEntry&& other);
  ~DohProviderEntry();

 private:
  DohProviderEntry(
      std::string provider,
      base::Optional<DohProviderIdForHistogram> provider_id_for_histogram,
      std::set<base::StringPiece> ip_strs,
      std::set<std::string> dns_over_tls_hostnames,
      std::string dns_over_https_template,
      std::string ui_name,
      std::string privacy_policy,
      bool display_globally,
      std::set<std::string> display_countries);
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_DOH_PROVIDER_ENTRY_H_
