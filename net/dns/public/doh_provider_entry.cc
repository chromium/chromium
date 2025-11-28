// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/doh_provider_entry.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/util.h"

namespace net {

namespace {

std::set<IPAddress> ParseIPs(
    const std::initializer_list<std::string_view>& ip_strs) {
  std::set<IPAddress> ip_addresses;
  for (std::string_view ip_str : ip_strs) {
    IPAddress ip_address;
    bool success = ip_address.AssignFromIPLiteral(ip_str);
    DCHECK(success);
    ip_addresses.insert(std::move(ip_address));
  }
  return ip_addresses;
}

DnsOverHttpsServerConfig ParseValidDohTemplate(
    std::string server_template,
    const std::initializer_list<std::string_view>& endpoint_ip_strs) {
  std::set<IPAddress> endpoint_ips = ParseIPs(endpoint_ip_strs);

  std::vector<std::vector<IPAddress>> endpoints;

  // Note: `DnsOverHttpsServerConfig` supports separate groups of endpoint IPs,
  // but for now we'll just support all endpoint IPs combined into one grouping
  // since the only use of the endpoint IPs in the server config combines them
  // anyway.
  if (!endpoint_ips.empty()) {
    endpoints.emplace_back(endpoint_ips.begin(), endpoint_ips.end());
  }

  auto parsed_template = DnsOverHttpsServerConfig::FromString(
      std::move(server_template), std::move(endpoints));
  DCHECK(parsed_template.has_value());  // Template must be valid.
  return std::move(*parsed_template);
}

}  // namespace

#define MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(feature_name, feature_state) \
  ([]() {                                                                  \
    static BASE_FEATURE(k##feature_name, #feature_name, feature_state);    \
    return &k##feature_name;                                               \
  })()

// static
const DohProviderEntry::List& DohProviderEntry::GetList() {
  // See /net/docs/adding_doh_providers.md for instructions on modifying this
  // DoH provider list.
  //
  // The provider names in these entries should be kept in sync with the
  // DohProviderId variant list in
  // tools/metrics/histograms/metadata/net/histograms.xml.
  static const auto entries(base::NoDestructor(std::to_array<DohProviderEntry>(
      {{"CleanBrowsingAdult",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCleanBrowsingAdult,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"185.228.168.10", "185.228.169.11", "2a0d:2a00:1::1",
         "2a0d:2a00:2::1"},
        /*dns_over_tls_hostnames=*/{"adult-filter-dns.cleanbrowsing.org"},
        "https://doh.cleanbrowsing.org/doh/adult-filter{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"CleanBrowsingFamily",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCleanBrowsingFamily,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"185.228.168.168", "185.228.169.168",
         "2a0d:2a00:1::", "2a0d:2a00:2::"},
        /*dns_over_tls_hostnames=*/{"family-filter-dns.cleanbrowsing.org"},
        "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
        /*ui_name=*/"CleanBrowsing (Family Filter)",
        /*privacy_policy=*/"https://cleanbrowsing.org/privacy",
        /*display_globally=*/true,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"CleanBrowsingSecure",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCleanBrowsingSecure,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"185.228.168.9", "185.228.169.9", "2a0d:2a00:1::2", "2a0d:2a00:2::2"},
        /*dns_over_tls_hostnames=*/{"security-filter-dns.cleanbrowsing.org"},
        "https://doh.cleanbrowsing.org/doh/security-filter{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Cloudflare",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCloudflare,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"1.1.1.1", "1.0.0.1", "2606:4700:4700::1111", "2606:4700:4700::1001"},
        /*dns_over_tls_hostnames=*/
        {"one.one.one.one", "1dot1dot1dot1.cloudflare-dns.com"},
        "https://chrome.cloudflare-dns.com/dns-query",
        /*ui_name=*/"Cloudflare (1.1.1.1)",
        "https://developers.cloudflare.com/1.1.1.1/privacy/"
        /*privacy_policy=*/"public-dns-resolver/",
        /*display_globally=*/true,
        /*display_countries=*/{},
        LoggingLevel::kExtra},
       {"Comcast",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderComcast,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"75.75.75.75", "75.75.76.76", "2001:558:feed::1", "2001:558:feed::2"},
        /*dns_over_tls_hostnames=*/{"dot.xfinity.com"},
        "https://doh.xfinity.com/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy*/ "",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kExtra},
       {"Cox",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCox,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"68.105.28.11", "68.105.28.12", "2001:578:3f::30"},
        /*dns_over_tls_hostnames=*/{"dot.cox.net"},
        "https://doh.cox.net/dns-query",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Cznic",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderCznic,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"185.43.135.1", "193.17.47.1", "2001:148f:fffe::1",
         "2001:148f:ffff::1"},
        /*dns_over_tls_hostnames=*/{"odvr.nic.cz"},
        "https://odvr.nic.cz/doh",
        /*ui_name=*/"CZ.NIC ODVR",
        /*privacy_policy=*/"https://www.nic.cz/odvr/",
        /*display_globally=*/false,
        /*display_countries=*/{"CZ"},
        LoggingLevel::kNormal},
       {"DNS4EU",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderDns4eu,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        /*dns_over_53_server_ip_strs=*/{},
        /*dns_over_tls_hostnames=*/{},
        "https://protective.joindns4.eu/dns-query{?dns}",
        /*ui_name=*/"DNS4EU Public Service (Protective)",
        /*privacy_policy=*/
        "https://app-eu1.hubspotdocuments.com/documents/142290803/view/"
        "1331719984?accessId=b505d9",
        /*display_globally=*/false,
        /*display_countries=*/{"AT", "BE", "BG", "HR", "CY", "CZ", "DK",
                               "EE", "FI", "FR", "DE", "GR", "HU", "IE",
                               "IT", "LV", "LT", "LU", "MT", "NL", "PL",
                               "PT", "RO", "SK", "SI", "ES", "SE"},
        LoggingLevel::kNormal,
        /*dns_over_https_server_ip_strs=*/
        {"86.54.11.1", "86.54.11.201", "2a13:1001::86:54:11:1",
         "2a13:1001::86:54:11:201"}},
       {"Dnssb",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderDnssb,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"185.222.222.222", "45.11.45.11", "2a09::", "2a11::"},
        /*dns_over_tls_hostnames=*/{"dns.sb"},
        "https://doh.dns.sb/dns-query{?dns}",
        /*ui_name=*/"DNS.SB",
        /*privacy_policy=*/"https://dns.sb/privacy/",
        /*display_globally=*/false,
        /*display_countries=*/{"EE", "DE"},
        LoggingLevel::kNormal},
       {"Google",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderGoogle,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"8.8.8.8", "8.8.4.4", "2001:4860:4860::8888", "2001:4860:4860::8844"},
        /*dns_over_tls_hostnames=*/
        {"dns.google", "dns.google.com", "8888.google"},
        "https://dns.google/dns-query{?dns}",
        /*ui_name=*/"Google (Public DNS)",
        "https://developers.google.com/speed/public-dns/"
        /*privacy_policy=*/"privacy",
        /*display_globally=*/true,
        /*display_countries=*/{},
        LoggingLevel::kExtra},
       {"GoogleDns64",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderGoogleDns64,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"2001:4860:4860::64", "2001:4860:4860::6464"},
        /*dns_over_tls_hostnames=*/{"dns64.dns.google"},
        "https://dns64.dns.google/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Iij",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderIij,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        /*dns_over_53_server_ip_strs=*/{},
        /*dns_over_tls_hostnames=*/{}, "https://public.dns.iij.jp/dns-query",
        /*ui_name=*/"IIJ (Public DNS)",
        /*privacy_policy=*/"https://policy.public.dns.iij.jp/",
        /*display_globally=*/false, /*display_countries=*/{"JP"},
        LoggingLevel::kNormal},
       {"Levonet",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderLevonet,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"109.236.119.2", "109.236.120.2", "2a02:6ca3:0:1::2",
         "2a02:6ca3:0:2::2"},
        /*dns_over_tls_hostnames=*/{},
        "https://dns.levonet.sk/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal,
        {"109.236.119.2", "109.236.120.2", "2a02:6ca3:0:1::2",
         "2a02:6ca3:0:2::2"}},
       {"NextDns",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderNextDns,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        /*dns_over_53_server_ip_strs=*/{},
        /*dns_over_tls_hostnames=*/{}, "https://chromium.dns.nextdns.io",
        /*ui_name=*/"NextDNS",
        /*privacy_policy=*/"https://nextdns.io/privacy",
        /*display_globally=*/false, /*display_countries=*/{"US"},
        LoggingLevel::kNormal},
       {"OpenDNS",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderOpenDNS,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"208.67.222.222", "208.67.220.220", "2620:119:35::35",
         "2620:119:53::53"},
        /*dns_over_tls_hostnames=*/{},
        "https://doh.opendns.com/dns-query{?dns}",
        /*ui_name=*/"OpenDNS",
        "https://www.cisco.com/c/en/us/about/legal/"
        /*privacy_policy=*/"privacy-full.html",
        /*display_globally=*/true,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"OpenDNSFamily",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderOpenDNSFamily,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"208.67.222.123", "208.67.220.123", "2620:119:35::123",
         "2620:119:53::123"},
        /*dns_over_tls_hostnames=*/{},
        "https://doh.familyshield.opendns.com/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Quad9Cdn",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderQuad9Cdn,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"9.9.9.11", "149.112.112.11", "2620:fe::11", "2620:fe::fe:11"},
        /*dns_over_tls_hostnames=*/{"dns11.quad9.net"},
        "https://dns11.quad9.net/dns-query",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Quad9Insecure",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderQuad9Insecure,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"9.9.9.10", "149.112.112.10", "2620:fe::10", "2620:fe::fe:10"},
        /*dns_over_tls_hostnames=*/{"dns10.quad9.net"},
        "https://dns10.quad9.net/dns-query",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Quad9Secure",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(
            DohProviderQuad9Secure, base::FEATURE_DISABLED_BY_DEFAULT),
        {"9.9.9.9", "149.112.112.112", "2620:fe::fe", "2620:fe::9"},
        /*dns_over_tls_hostnames=*/{"dns.quad9.net", "dns9.quad9.net"},
        "https://dns.quad9.net/dns-query",
        /*ui_name=*/"Quad9 (9.9.9.9)",
        /*privacy_policy=*/"https://www.quad9.net/home/privacy/",
        /*display_globally=*/true,
        /*display_countries=*/{},
        LoggingLevel::kExtra},
       {"Quickline",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderQuickline,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"212.60.61.246", "212.60.63.246", "2001:1a88:10:ffff::1",
         "2001:1a88:10:ffff::2"},
        /*dns_over_tls_hostnames=*/{"dot.quickline.ch"},
        "https://doh.quickline.ch/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Spectrum1",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderSpectrum1,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
         "2001:1998:0f00:0002::1"},
        /*dns_over_tls_hostnames=*/{},
        "https://doh-01.spectrum.com/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal},
       {"Spectrum2",
        MAKE_BASE_FEATURE_WITH_STATIC_STORAGE(DohProviderSpectrum2,
                                              base::FEATURE_ENABLED_BY_DEFAULT),
        {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
         "2001:1998:0f00:0002::1"},
        /*dns_over_tls_hostnames=*/{},
        "https://doh-02.spectrum.com/dns-query{?dns}",
        /*ui_name=*/"",
        /*privacy_policy=*/"",
        /*display_globally=*/false,
        /*display_countries=*/{},
        LoggingLevel::kNormal}})));

  static const base::NoDestructor<DohProviderEntry::List> providers(
      base::ToVector(*entries, [](const DohProviderEntry& entry) {
        return raw_ptr<const DohProviderEntry, VectorExperimental>(&entry);
      }));
  return *providers;
}

#undef MAKE_BASE_FEATURE_WITH_STATIC_STORAGE

// static
DohProviderEntry DohProviderEntry::ConstructForTesting(
    std::string_view provider,
    const base::Feature* feature,
    std::initializer_list<std::string_view> dns_over_53_server_ip_strs,
    base::flat_set<std::string_view> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string_view ui_name,
    std::string_view privacy_policy,
    bool display_globally,
    base::flat_set<std::string> display_countries,
    LoggingLevel logging_level) {
  return DohProviderEntry(provider, feature, dns_over_53_server_ip_strs,
                          std::move(dns_over_tls_hostnames),
                          std::move(dns_over_https_template), ui_name,
                          privacy_policy, display_globally,
                          std::move(display_countries), logging_level);
}

DohProviderEntry::~DohProviderEntry() = default;

DohProviderEntry::DohProviderEntry(
    std::string_view provider,
    const base::Feature* feature,
    std::initializer_list<std::string_view> dns_over_53_server_ip_strs,
    base::flat_set<std::string_view> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string_view ui_name,
    std::string_view privacy_policy,
    bool display_globally,
    base::flat_set<std::string> display_countries,
    LoggingLevel logging_level,
    std::initializer_list<std::string_view> dns_over_https_server_ip_strs)
    : provider(provider),
      feature(*feature),
      ip_addresses(ParseIPs(dns_over_53_server_ip_strs)),
      dns_over_tls_hostnames(std::move(dns_over_tls_hostnames)),
      doh_server_config(
          ParseValidDohTemplate(std::move(dns_over_https_template),
                                dns_over_https_server_ip_strs)),
      ui_name(ui_name),
      privacy_policy(privacy_policy),
      display_globally(display_globally),
      display_countries(std::move(display_countries)),
      logging_level(logging_level) {
  DCHECK(!display_globally || this->display_countries.empty());
  if (display_globally || !this->display_countries.empty()) {
    DCHECK(!this->ui_name.empty());
    DCHECK(!this->privacy_policy.empty());
  }
  for (const auto& display_country : this->display_countries) {
    DCHECK_EQ(2u, display_country.size());
  }
}

DohProviderEntry::DohProviderEntry(DohProviderEntry&& other) = default;
}  // namespace net
