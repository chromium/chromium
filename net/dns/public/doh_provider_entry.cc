// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/doh_provider_entry.h"

#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
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

}  // namespace

// static
const DohProviderEntry::List& DohProviderEntry::GetList() {
  // The provider names in these entries should be kept in sync with the
  // DohProviderId histogram suffix list in
  // tools/metrics/histograms/histograms.xml.
  static const base::NoDestructor<DohProviderEntry::List> providers{{
      new DohProviderEntry("AlekBergNl", DohProviderIdForHistogram::kAlekBergNl,
                           {} /* ip_strs */, {} /* dns_over_tls_hostnames */,
                           "https://dnsnl.alekberg.net/dns-query{?dns}",
                           "alekberg.net (NL)" /* ui_name */,
                           "https://alekberg.net/privacy" /* privacy_policy */,
                           false /* display_globally */,
                           {"NL"} /* display_countries */),
      new DohProviderEntry(
          "CleanBrowsingAdult", base::nullopt /* provider_id_for_histogram */,
          {"185.228.168.10", "185.228.169.11", "2a0d:2a00:1::1",
           "2a0d:2a00:2::1"},
          {"adult-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/adult-filter{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "CleanBrowsingFamily",
          DohProviderIdForHistogram::kCleanBrowsingFamily,
          {"185.228.168.168", "185.228.169.168",
           "2a0d:2a00:1::", "2a0d:2a00:2::"},
          {"family-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
          "CleanBrowsing (Family Filter)" /* ui_name */,
          "https://cleanbrowsing.org/privacy" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "CleanBrowsingSecure", base::nullopt /* provider_id_for_histogram */,
          {"185.228.168.9", "185.228.169.9", "2a0d:2a00:1::2",
           "2a0d:2a00:2::2"},
          {"security-filter-dns.cleanbrowsing.org"} /* dot_hostnames */,
          "https://doh.cleanbrowsing.org/doh/security-filter{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */),
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
          true /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "Comcast", base::nullopt /* provider_id_for_histogram */,
          {"75.75.75.75", "75.75.76.76", "2001:558:feed::1",
           "2001:558:feed::2"},
          {"dot.xfinity.com"} /* dns_over_tls_hostnames */,
          "https://doh.xfinity.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Cznic", DohProviderIdForHistogram::kCznic,
          {"185.43.135.1", "2001:148f:fffe::1"},
          {"odvr.nic.cz"} /* dns_over_tls_hostnames */,
          "https://odvr.nic.cz/doh", "CZ.NIC ODVR" /* ui_name */,
          "https://www.nic.cz/odvr/" /* privacy_policy */,
          false /* display_globally */, {"CZ"} /* display_countries */),
      // Note: DNS.SB has separate entries for autoupgrade and settings UI to
      // allow the extra |no_ecs| parameter for autoupgrade. This parameter
      // disables EDNS Client Subnet (ECS) handling in order to match the
      // behavior of the upgraded-from classic DNS server.
      new DohProviderEntry(
          "Dnssb", base::nullopt /* provider_id_for_histogram */,
          {"185.222.222.222", "185.184.222.222", "2a09::", "2a09::1"},
          {"dns.sb"} /* dns_over_tls_hostnames */,
          "https://doh.dns.sb/dns-query?no_ecs=true{&dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "DnssbUserSelected", DohProviderIdForHistogram::kDnsSb,
          {} /* ip_strs */, {} /* dns_over_tls_hostnames */,
          "https://doh.dns.sb/dns-query{?dns}", "DNS.SB" /* ui_name */,
          "https://dns.sb/privacy/" /* privacy_policy */,
          false /* display_globally */, {"EE", "DE"} /* display_countries */),
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
                           {} /* display_countries */),
      new DohProviderEntry(
          "GoogleDns64", base::nullopt /* provider_id_for_histogram */,
          {"2001:4860:4860::64", "2001:4860:4860::6464"},
          {"dns64.dns.google"} /* dns_over_tls_hostnames */,
          "https://dns64.dns.google/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry("Iij", DohProviderIdForHistogram::kIij,
                           {} /* ip_strs */, {} /* dns_over_tls_hostnames */,
                           "https://public.dns.iij.jp/dns-query",
                           "IIJ (Public DNS)" /* ui_name */,
                           "https://public.dns.iij.jp/" /* privacy_policy */,
                           false /* display_globally */,
                           {"JP"} /* display_countries */),
      new DohProviderEntry(
          "NextDns", DohProviderIdForHistogram::kNextDns, {} /* ip_strs */,
          {} /* dns_over_tls_hostnames */, "https://chromium.dns.nextdns.io",
          "NextDNS" /* ui_name */,
          "https://nextdns.io/privacy" /* privacy_policy */,
          false /* display_globally */, {"US"} /* display_countries */),
      new DohProviderEntry(
          "OpenDNS", DohProviderIdForHistogram::kOpenDns,
          {"208.67.222.222", "208.67.220.220", "2620:119:35::35",
           "2620:119:53::53"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh.opendns.com/dns-query{?dns}", "OpenDNS" /* ui_name */,
          "https://www.cisco.com/c/en/us/about/legal/"
          "privacy-full.html" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "OpenDNSFamily", base::nullopt /* provider_id_for_histogram */,
          {"208.67.222.123", "208.67.220.123", "2620:119:35::123",
           "2620:119:53::123"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh.familyshield.opendns.com/dns-query{?dns}",
          "" /* ui_name */, "" /* privacy_policy */,
          false /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "Quad9Cdn", base::nullopt /* provider_id_for_histogram */,
          {"9.9.9.11", "149.112.112.11", "2620:fe::11", "2620:fe::fe:11"},
          {"dns11.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns11.quad9.net/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Quad9Insecure", base::nullopt /* provider_id_for_histogram */,
          {"9.9.9.10", "149.112.112.10", "2620:fe::10", "2620:fe::fe:10"},
          {"dns10.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns10.quad9.net/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Quad9Secure", DohProviderIdForHistogram::kQuad9Secure,
          {"9.9.9.9", "149.112.112.112", "2620:fe::fe", "2620:fe::9"},
          {"dns.quad9.net", "dns9.quad9.net"} /* dns_over_tls_hostnames */,
          "https://dns.quad9.net/dns-query", "Quad9 (9.9.9.9)" /* ui_name */,
          "https://www.quad9.net/home/privacy/" /* privacy_policy */,
          true /* display_globally */, {} /* display_countries */),
      new DohProviderEntry(
          "Quickline", base::nullopt /* provider_id_for_histogram */,
          {"212.60.61.246", "212.60.63.246", "2001:1a88:10:ffff::1",
           "2001:1a88:10:ffff::2"},
          {"dot.quickline.ch"} /* dns_over_tls_hostnames */,
          "https://doh.quickline.ch/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Spectrum1", base::nullopt /* provider_id_for_histogram */,
          {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
           "2001:1998:0f00:0002::1"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh-01.spectrum.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Spectrum2", base::nullopt /* provider_id_for_histogram */,
          {"209.18.47.61", "209.18.47.62", "2001:1998:0f00:0001::1",
           "2001:1998:0f00:0002::1"},
          {""} /* dns_over_tls_hostnames */,
          "https://doh-02.spectrum.com/dns-query{?dns}", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
      new DohProviderEntry(
          "Switch", base::nullopt /* provider_id_for_histogram */,
          {"130.59.31.251", "130.59.31.248", "2001:620:0:ff::2",
           "2001:620:0:ff::3"},
          {"dns.switch.ch"} /* dns_over_tls_hostnames */,
          "https://dns.switch.ch/dns-query", "" /* ui_name */,
          "" /* privacy_policy */, false /* display_globally */,
          {} /* display_countries */),
  }};
  return *providers;
}

// static
DohProviderEntry DohProviderEntry::ConstructForTesting(
    std::string provider,
    base::Optional<DohProviderIdForHistogram> provider_id_for_histogram,
    std::set<base::StringPiece> ip_strs,
    std::set<std::string> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string ui_name,
    std::string privacy_policy,
    bool display_globally,
    std::set<std::string> display_countries) {
  return DohProviderEntry(provider, provider_id_for_histogram, ip_strs,
                          dns_over_tls_hostnames, dns_over_https_template,
                          ui_name, privacy_policy, display_globally,
                          display_countries);
}

DohProviderEntry::DohProviderEntry(DohProviderEntry&& other) = default;
DohProviderEntry& DohProviderEntry::operator=(DohProviderEntry&& other) =
    default;

DohProviderEntry::~DohProviderEntry() = default;

DohProviderEntry::DohProviderEntry(
    std::string provider,
    base::Optional<DohProviderIdForHistogram> provider_id_for_histogram,
    std::set<base::StringPiece> ip_strs,
    std::set<std::string> dns_over_tls_hostnames,
    std::string dns_over_https_template,
    std::string ui_name,
    std::string privacy_policy,
    bool display_globally,
    std::set<std::string> display_countries)
    : provider(std::move(provider)),
      provider_id_for_histogram(std::move(provider_id_for_histogram)),
      ip_addresses(ParseIPs(ip_strs)),
      dns_over_tls_hostnames(std::move(dns_over_tls_hostnames)),
      dns_over_https_template(std::move(dns_over_https_template)),
      ui_name(std::move(ui_name)),
      privacy_policy(std::move(privacy_policy)),
      display_globally(display_globally),
      display_countries(std::move(display_countries)) {
  DCHECK(!this->dns_over_https_template.empty());
  DCHECK(dns_util::IsValidDohTemplate(this->dns_over_https_template,
                                      nullptr /* server_method */));

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
