// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/doh_provider_entry.h"

#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/util.h"

namespace net {

namespace {

std::set<IPAddress> ParseIPs(const std::set<base::StringPiece>& ip_strs) {
  std::set<IPAddress> ip_addresses;
  for (base::StringPiece ip_str : ip_strs) {
    IPAddress ip_address;
    bool success = ip_address.AssignFromIPLiteral(ip_str);
    DCHECK(success);
    ip_addresses.insert(std::move(ip_address));
  }
  return ip_addresses;
}

DnsOverHttpsServerConfig ParseValidDohTemplate(std::string server_template) {
  auto parsed_template =
      DnsOverHttpsServerConfig::FromString(std::move(server_template));
  DCHECK(parsed_template.has_value());  // Template must be valid.
  return std::move(*parsed_template);
}

}  // namespace

// static
const DohProviderEntry::List& DohProviderEntry::GetList() {
  // See /net/docs/adding_doh_providers.md for instructions on modifying this
  // DoH provider list.
  //
  // The provider names in these entries should be kept in sync with the
  // DohProviderId histogram suffix list in
  // tools/metrics/histograms/metadata/histogram_suffixes_list.xml.
  static const base::NoDestructor<DohProviderEntry::List> providers{{
      new DohProviderEntry("AlekBergNl", DohProviderIdForHistogram::kAlekBergNl,
                           {} /* ip_strs */, {} /* dns_over_tls_hostnames */,
                           "https://dnsnl.alekberg.net/dns-query{?dns}",
                           "alekberg.net (NL)" /* ui_name */,
                           "https://alekberg.net/privacy" /* privacy_policy */,
                           false /* display_globally */,
                           {"NL"} /* display_countries */,
                           LoggingLevel::kNormal),
      new DohProviderEntry(
          "CleanBrowsingAdult", absl::nullopt /* provider_id_for_histogram */,
          {"185.228.168.10", "185.228.169.11", "2a0d:2a00:1::1",
           "2a0d:2a00:2::1"},
          {"adult-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/adult-filter{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry(
          "CleanBrowsingFamily",
          DohProviderIdForHistogram::kCleanBrowsingFamily,
          {"185.228.168.168", "185.228.169.168",
           "2a0d:2a00:1::", "2a0d:2a00:2::"},
          {"family-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
          "CleanBrowsing (Family Filter)" /* ui_name */,
          "https://cleanbrowsing.org/privacy" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry(
          "CleanBrowsingSecure", absl::nullopt /* provider_id_for_histogram */,
          {"185.228.168.9", "185.228.169.9", "2a0d:2a00:1::2",
           "2a0d:2a00:2::2"},
          {"security-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/security-filter{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry(
          "Cloudflare", DohProviderIdForHistogram::kCloudflare,
          {"1.1.1.1", "1.0.0.1", "2606:4700:4700::1111",
           "2606:4700:4700::1001"},
          {"one.one.one.one",
           "1dot1dot1dot1.cloudflare-dns.com"} /* dns_over_tls_hostnames */,
          "https://chrome.cloudflare-dns.com/dns-query",
          "Cloudflare (1.1.1.1)" /* ui_name */,
          "https://developers.cloudflare.com/1.1.1.1/privacy/"
          "public-dns-resolver/" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */,
          LoggingLevel::kExtra),
      new DohProviderEntry(
          "Comcast", absl::nullopt /* provider_id_for_histogram */,
          {"75.75.75.75", "75.75.76.76", "2001:558:feed::1",
           "2001:558:feed::2"},
          {"dot.xfinity.com"} /* dns_over_tls_hostnames */,
          "https://doh.xfinity.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kExtra),
      new DohProviderEntry("Cox", /*provider_id_for_histogram=*/absl::nullopt,
                           {"68.105.28.11", "68.105.28.12", "2001:578:3f::30"},
                           /*dns_over_tls_hostnames=*/{"dot.cox.net"},
                           "https://doh.cox.net/dns-query",
                           /*ui_name=*/"", /*privacy_policy=*/"",
                           /*display_globally=*/false, /*display_countries=*/{},
                           LoggingLevel::kNormal),
      new DohProviderEntry(
          "Cznic", DohProviderIdForHistogram::kCznic,
          {"185.43.135.1", "193.17.47.1", "2001:148f:fffe::1",
           "2001:148f:ffff::1"},
          {"odvr.nic.cz"} /* dns_over_tls_hostnames */,
          "https://odvr.nic.cz/doh", "CZ.NIC ODVR" /* ui_name */,
          "https://www.nic.cz/odvr/" /* privacy_policy */,
          false /* display_globally */, {"CZ"} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry(
          "Dnssb", DohProviderIdForHistogram::kDnsSb,
          {"185.222.222.222", "45.11.45.11", "2a09::", "2a11::"},
          /*dns_over_tls_hostnames=*/{"dns.sb"},
          "https://doh.dns.sb/dns-query{?dns}", /*ui_name=*/"DNS.SB",
          /*privacy_policy=*/"https://dns.sb/privacy/",
          /*display_globally=*/false, /*display_countries=*/{"EE", "DE"},
          LoggingLevel::kNormal),
      new DohProviderEntry("Google", DohProviderIdForHistogram::kGoogle,
                           {"8.8.8.8", "8.8.4.4", "2001:4860:4860::8888",
                            "2001:4860:4860::8844"},
                           {"dns.google", "dns.google.com",
                            "8888.google"} /* dns_over_tls_hostnames */,
                           "https://dns.google/dns-query{?dns}",
                           "Google (Public DNS)" /* ui_name */,
                           "https://developers.google.com/speed/public-dns/"
                           "privacy" /* privacy_policy */,
                           true /* display_globally */,
                           {} /* display_countries */, LoggingLevel::kExtra),
      new DohProviderEntry(
          "GoogleDns64", absl::nullopt /* provider_id_for_histogram */,
          {"2001:4860:4860::64", "2001:4860:4860::6464"},
          {"dns64.dns.google"} /* dns_over_tls_hostnames */,
          "https://dns64.dns.google/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry("Iij", DohProviderIdForHistogram::kIij,
                           {} /* ip_strs */, {} /* dns_over_tls_hostnames */,
                           "https://public.dns.iij.jp/dns-query",
                           "IIJ (Public DNS)" /* ui_name */,
                           "https://public.dns.iij.jp/" /* privacy_policy */,
                           false /* display_globally */,
                           {"JP"} /* display_countries */,
                           LoggingLevel::kNormal),
      new DohProviderEntry(
          "NextDns", DohProviderIdForHistogram::kNextDns, {} /* ip_strs */,
          {} /* dns_over_tls_hostnames */, "https://chromium.dns.nextdns.io",
          "NextDNS" /* ui_name */,
          "https://nextdns.io/privacy" /* privacy_policy */,
          false /* display_globally */, {"US"} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry("OpenDNS", DohProviderIdForHistogram::kOpenDns,
                           {"208.67.222.222", "208.67.220.220",
                            "2620:119:35::35", "2620:119:53::53"},
                           {""} /* dns_over_tls_hostnames */,
                           "https://doh.opendns.com/dns-query{?dns}",
                           "OpenDNS" /* ui_name */,
                           "https://www.cisco.com/c/en/us/about/legal/"
                           "privacy-full.html" /* privacy_policy */,
                           true /* display_globally */,
                           {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "OpenDNSFamily", absl::nullopt /* provider_id_for_histogram */,
          {"208.67.222.123", "208.67.220.123", "2620:119:35::123",
           "2620:119:53::123"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh.familyshield.opendns.com/dns-query{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */,
          LoggingLevel::kNormal),
      new DohProviderEntry(
          "Quad9Cdn", absl::nullopt /* provider_id_for_histogram */,
          {"9.9.9.11", "149.112.112.11", "2620:fe::11", "2620:fe::fe:11"},
          {"dns11.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns11.quad9.net/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "Quad9Insecure", absl::nullopt /* provider_id_for_histogram */,
          {"9.9.9.10", "149.112.112.10", "2620:fe::10", "2620:fe::fe:10"},
          {"dns10.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns10.quad9.net/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "Quad9Secure", DohProviderIdForHistogram::kQuad9Secure,
          {"9.9.9.9", "149.112.112.112", "2620:fe::fe", "2620:fe::9"},
          {"dns.quad9.net", "dns9.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns.quad9.net/dns-query", "Quad9 (9.9.9.9)" /* ui_name */,
          "https://www.quad9.net/home/privacy/" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */,
          LoggingLevel::kExtra),
      new DohProviderEntry(
          "Quickline", absl::nullopt /* provider_id_for_histogram */,
          {"212.60.61.246", "212.60.63.246", "2001:1a88:10:ffff::1",
           "2001:1a88:10:ffff::2"},
          {"dot.quickline.ch"} /* dns_over_tls_hostnames */,
          "https://doh.quickline.ch/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "Spectrum1", absl::nullopt /* provider_id_for_histogram */,
          {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
           "2001:1998:0f00:0002::1"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh-01.spectrum.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "Spectrum2", absl::nullopt /* provider_id_for_histogram */,
          {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
           "2001:1998:0f00:0002::1"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh-02.spectrum.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
      new DohProviderEntry(
          "Switch", absl::nullopt /* provider_id_for_histogram */,
          {"130.59.31.251", "130.59.31.248", "2001:620:0:ff::2",
           "2001:620:0:ff::3"},
          {"dns.switch.ch"} /* dns_over_tls_hostnames */,
          "https://dns.switch.ch/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */, LoggingLevel::kNormal),
  }};
  return *providers;
}

// static
DohProviderEntry DohProviderEntry::ConstructForTesting(
    std::string provider,
    absl::optional<DohProviderIdForHistogram> provider_id_for_histogram,
    std::set<base::StringPiece> ip_strs,
    std::set<std::string> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string ui_name,
    std::string privacy_policy,
    bool display_globally,
    std::set<std::string> display_countries,
    LoggingLevel logging_level) {
  return DohProviderEntry(provider, provider_id_for_histogram, ip_strs,
                          dns_over_tls_hostnames, dns_over_https_template,
                          ui_name, privacy_policy, display_globally,
                          display_countries, logging_level);
}

DohProviderEntry::DohProviderEntry(DohProviderEntry&& other) = default;
DohProviderEntry& DohProviderEntry::operator=(DohProviderEntry&& other) =
    default;

DohProviderEntry::~DohProviderEntry() = default;

DohProviderEntry::DohProviderEntry(
    std::string provider,
    absl::optional<DohProviderIdForHistogram> provider_id_for_histogram,
    std::set<base::StringPiece> ip_strs,
    std::set<std::string> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string ui_name,
    std::string privacy_policy,
    bool display_globally,
    std::set<std::string> display_countries,
    LoggingLevel logging_level)
    : provider(std::move(provider)),
      provider_id_for_histogram(std::move(provider_id_for_histogram)),
      ip_addresses(ParseIPs(ip_strs)),
      dns_over_tls_hostnames(std::move(dns_over_tls_hostnames)),
      doh_server_config(
          ParseValidDohTemplate(std::move(dns_over_https_template))),
      ui_name(std::move(ui_name)),
      privacy_policy(std::move(privacy_policy)),
      display_globally(display_globally),
      display_countries(std::move(display_countries)),
      logging_level(logging_level) {
  DCHECK(!display_globally || this->display_countries.empty());
  if (display_globally || !this->display_countries.empty()) {
    DCHECK(!this->ui_name.empty());
    DCHECK(!this->privacy_policy.empty());
    DCHECK(this->provider_id_for_histogram.has_value());
  }
  for (const auto& display_country : this->display_countries) {
    DCHECK_EQ(2u, display_country.size());
  }
}

}  // namespace net
