// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_util.h"

#include "base/stl_util.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class DNSUtilTest : public testing::Test {
};

// IncludeNUL converts a char* to a std::string and includes the terminating
// NUL in the result.
static std::string IncludeNUL(const char* in) {
  return std::string(in, strlen(in) + 1);
}

TEST_F(DNSUtilTest, DNSDomainFromDot) {
  std::string out;

  EXPECT_FALSE(DNSDomainFromDot("", &out));
  EXPECT_FALSE(DNSDomainFromDot(".", &out));
  EXPECT_FALSE(DNSDomainFromDot("..", &out));
  EXPECT_FALSE(DNSDomainFromDot("foo,bar.com", &out));

  EXPECT_TRUE(DNSDomainFromDot("com", &out));
  EXPECT_EQ(out, IncludeNUL("\003com"));
  EXPECT_TRUE(DNSDomainFromDot("google.com", &out));
  EXPECT_EQ(out, IncludeNUL("\x006google\003com"));
  EXPECT_TRUE(DNSDomainFromDot("www.google.com", &out));
  EXPECT_EQ(out, IncludeNUL("\003www\006google\003com"));

  // Label is 63 chars: still valid
  EXPECT_TRUE(DNSDomainFromDot("z23456789a123456789a123456789a123456789a123456789a123456789a123", &out));
  EXPECT_EQ(out, IncludeNUL("\077z23456789a123456789a123456789a123456789a123456789a123456789a123"));

  // Label is too long: invalid
  EXPECT_FALSE(DNSDomainFromDot("123456789a123456789a123456789a123456789a123456789a123456789a1234", &out));

  // 253 characters in the name: still valid
  EXPECT_TRUE(DNSDomainFromDot("abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abc", &out));
  EXPECT_EQ(out, IncludeNUL("\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\011abcdefghi\003abc"));

  // 254 characters in the name: invalid
  EXPECT_FALSE(DNSDomainFromDot("123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.1234", &out));

  // Zero length labels should fail, except that one trailing dot is allowed
  // (to disable suffix search):
  EXPECT_FALSE(DNSDomainFromDot(".google.com", &out));
  EXPECT_FALSE(DNSDomainFromDot("www..google.com", &out));

  EXPECT_TRUE(DNSDomainFromDot("www.google.com.", &out));
  EXPECT_EQ(out, IncludeNUL("\003www\006google\003com"));

  // Spaces and parenthesis not permitted.
  EXPECT_FALSE(DNSDomainFromDot("_ipp._tcp.local.foo printer (bar)", &out));
}

TEST_F(DNSUtilTest, DNSDomainFromUnrestrictedDot) {
  std::string out;

  // Spaces and parentheses allowed.
  EXPECT_TRUE(
      DNSDomainFromUnrestrictedDot("_ipp._tcp.local.foo printer (bar)", &out));
  EXPECT_EQ(out, IncludeNUL("\004_ipp\004_tcp\005local\021foo printer (bar)"));

  // Standard dotted domains still work correctly.
  EXPECT_TRUE(DNSDomainFromUnrestrictedDot("www.google.com", &out));
  EXPECT_EQ(out, IncludeNUL("\003www\006google\003com"));

  // Label is too long: invalid
  EXPECT_FALSE(DNSDomainFromUnrestrictedDot(
      "123456789a123456789a123456789a123456789a123456789a123456789a1234",
      &out));
}

TEST_F(DNSUtilTest, DNSDomainToString) {
  EXPECT_EQ("", DNSDomainToString(IncludeNUL("")));
  EXPECT_EQ("foo", DNSDomainToString(IncludeNUL("\003foo")));
  EXPECT_EQ("foo.bar", DNSDomainToString(IncludeNUL("\003foo\003bar")));
  EXPECT_EQ("foo.bar.uk",
            DNSDomainToString(IncludeNUL("\003foo\003bar\002uk")));

  // It should cope with a lack of root label.
  EXPECT_EQ("foo.bar", DNSDomainToString("\003foo\003bar"));

  // Invalid inputs should return an empty string.
  EXPECT_EQ("", DNSDomainToString(IncludeNUL("\x80")));
  EXPECT_EQ("", DNSDomainToString("\x06"));
}

TEST_F(DNSUtilTest, IsValidDNSDomain) {
  const char* const bad_hostnames[] = {
      "%20%20noodles.blorg", "noo dles.blorg ",    "noo dles.blorg. ",
      "^noodles.blorg",      "noodles^.blorg",     "noo&dles.blorg",
      "noodles.blorg`",      "www.-noodles.blorg",
  };

  for (size_t i = 0; i < base::size(bad_hostnames); ++i) {
    EXPECT_FALSE(IsValidDNSDomain(bad_hostnames[i]));
  }

  const char* const good_hostnames[] = {
      "www.noodles.blorg",   "1www.noodles.blorg", "www.2noodles.blorg",
      "www.n--oodles.blorg", "www.noodl_es.blorg", "www.no-_odles.blorg",
      "www_.noodles.blorg",  "www.noodles.blorg.", "_privet._tcp.local",
  };

  for (size_t i = 0; i < base::size(good_hostnames); ++i) {
    EXPECT_TRUE(IsValidDNSDomain(good_hostnames[i]));
  }
}

TEST_F(DNSUtilTest, IsValidUnrestrictedDNSDomain) {
  const char* const good_hostnames[] = {
      "www.noodles.blorg",   "1www.noodles.blorg",    "www.2noodles.blorg",
      "www.n--oodles.blorg", "www.noodl_es.blorg",    "www.no-_odles.blorg",
      "www_.noodles.blorg",  "www.noodles.blorg.",    "_privet._tcp.local",
      "%20%20noodles.blorg", "noo dles.blorg ",       "noo dles_ipp._tcp.local",
      "www.nood(les).blorg", "noo dl(es)._tcp.local",
  };

  for (size_t i = 0; i < base::size(good_hostnames); ++i) {
    EXPECT_TRUE(IsValidUnrestrictedDNSDomain(good_hostnames[i]));
  }
}

TEST_F(DNSUtilTest, GetURLFromTemplateWithoutParameters) {
  EXPECT_EQ("https://dnsserver.example.net/dns-query",
            GetURLFromTemplateWithoutParameters(
                "https://dnsserver.example.net/dns-query{?dns}"));
}

TEST_F(DNSUtilTest, GetDohUpgradeServersFromDotHostname) {
  std::vector<DnsConfig::DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromDotHostname("", std::vector<std::string>());
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname("unrecognized",
                                                    std::vector<std::string>());
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org", std::vector<std::string>());
  EXPECT_EQ(1u, doh_servers.size());
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
            doh_servers[0].server_template);

  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org",
      std::vector<std::string>({"CleanBrowsingFamily"}));
  EXPECT_EQ(0u, doh_servers.size());
}

TEST_F(DNSUtilTest, GetDohUpgradeServersFromNameservers) {
  std::vector<IPEndPoint> nameservers;
  // Cloudflare upgradeable IPs
  IPAddress dns_ip0(1, 0, 0, 1);
  IPAddress dns_ip1;
  EXPECT_TRUE(dns_ip1.AssignFromIPLiteral("2606:4700:4700::1111"));
  // SafeBrowsing family filter upgradeable IP
  IPAddress dns_ip2;
  EXPECT_TRUE(dns_ip2.AssignFromIPLiteral("2a0d:2a00:2::"));
  // SafeBrowsing security filter upgradeable IP
  IPAddress dns_ip3(185, 228, 169, 9);
  // None-upgradeable IP
  IPAddress dns_ip4(1, 2, 3, 4);

  nameservers.push_back(IPEndPoint(dns_ip0, dns_protocol::kDefaultPort));
  nameservers.push_back(IPEndPoint(dns_ip1, dns_protocol::kDefaultPort));
  nameservers.push_back(IPEndPoint(dns_ip2, 54));
  nameservers.push_back(IPEndPoint(dns_ip3, dns_protocol::kDefaultPort));
  nameservers.push_back(IPEndPoint(dns_ip4, dns_protocol::kDefaultPort));

  std::vector<DnsConfig::DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromNameservers(std::vector<IPEndPoint>(),
                                          std::vector<std::string>());
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromNameservers(nameservers,
                                                    std::vector<std::string>());
  EXPECT_EQ(3u, doh_servers.size());
  EXPECT_EQ("https://chrome.cloudflare-dns.com/dns-query",
            doh_servers[0].server_template);
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
            doh_servers[1].server_template);
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/security-filter{?dns}",
            doh_servers[2].server_template);

  doh_servers = GetDohUpgradeServersFromNameservers(
      nameservers, std::vector<std::string>(
                       {"CleanBrowsingSecure", "Cloudflare", "Unexpected"}));
  EXPECT_EQ(1u, doh_servers.size());
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
            doh_servers[0].server_template);
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromDohConfig) {
  EXPECT_EQ("Cloudflare", GetDohProviderIdForHistogramFromDohConfig(
                              {"https://chrome.cloudflare-dns.com/dns-query",
                               true /* use_post */}));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromDohConfig(
                         {"https://unexpected.dohserver.com/dns-query",
                          true /* use_post */}));
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromNameserver) {
  EXPECT_EQ("CleanBrowsingSecure",
            GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                IPAddress(185, 228, 169, 9), dns_protocol::kDefaultPort)));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                         IPAddress(1, 2, 3, 4), dns_protocol::kDefaultPort)));
}

}  // namespace net
