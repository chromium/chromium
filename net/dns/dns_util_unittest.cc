// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_util.h"

#include <limits.h>
#include <stdint.h>

#include <string>

#include "base/big_endian.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/doh_provider_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {
// Returns the DoH provider entry in `DohProviderEntry::GetList()` that matches
// `provider`. Crashes if there is no matching entry.
const DohProviderEntry& GetDohProviderEntry(base::StringPiece provider) {
  auto provider_list = DohProviderEntry::GetList();
  auto it =
      base::ranges::find(provider_list, provider, &DohProviderEntry::provider);
  CHECK(it != provider_list.end());
  return **it;
}
}  // namespace

using testing::Eq;

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

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleSimpleNames) {
  std::string dns_name = "\003foo";
  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("foo")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader), testing::Optional(Eq("foo")));

  dns_name += "\003bar";
  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("foo.bar")));
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader1), testing::Optional(Eq("foo.bar")));

  dns_name += "\002uk";
  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("foo.bar.uk")));
  auto reader2 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader2), testing::Optional(Eq("foo.bar.uk")));

  dns_name += '\0';
  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("foo.bar.uk")));
  auto reader3 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader3), testing::Optional(Eq("foo.bar.uk")));
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleEmpty) {
  std::string dns_name;

  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader), testing::Optional(Eq("")));

  dns_name += '\0';

  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("")));
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader1), testing::Optional(Eq("")));
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectEmptyIncomplete) {
  std::string dns_name;

  EXPECT_THAT(DnsDomainToString(dns_name, false /* require_complete */),
              testing::Optional(Eq("")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader, false /* require_complete */),
              testing::Optional(Eq("")));

  EXPECT_EQ(DnsDomainToString(dns_name, true /* require_complete */),
            absl::nullopt);
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader1, true /* require_complete */),
            absl::nullopt);
}

// Test `require_complete` functionality given an input with terminating zero-
// length label.
TEST_F(DNSUtilTest, DnsDomainToStringComplete) {
  std::string dns_name("\003foo\004test");
  dns_name += '\0';

  EXPECT_THAT(DnsDomainToString(dns_name, false /* require_complete */),
              testing::Optional(Eq("foo.test")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader, false /* require_complete */),
              testing::Optional(Eq("foo.test")));

  EXPECT_THAT(DnsDomainToString(dns_name, true /* require_complete */),
              testing::Optional(Eq("foo.test")));
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader1, true /* require_complete */),
              testing::Optional(Eq("foo.test")));
}

// Test `require_complete` functionality given an input without terminating
// zero-length label.
TEST_F(DNSUtilTest, DnsDomainToStringNotComplete) {
  std::string dns_name("\003boo\004test");

  EXPECT_THAT(DnsDomainToString(dns_name, false /* require_complete */),
              testing::Optional(Eq("boo.test")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader, false /* require_complete */),
              testing::Optional(Eq("boo.test")));

  EXPECT_EQ(DnsDomainToString(dns_name, true /* require_complete */),
            absl::nullopt);
  auto reader2 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader2, true /* require_complete */),
            absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectEmptyWhenRequiringComplete) {
  std::string dns_name;

  EXPECT_THAT(DnsDomainToString(dns_name, false /* require_complete */),
              testing::Optional(Eq("")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader, false /* require_complete */),
              testing::Optional(Eq("")));

  EXPECT_EQ(DnsDomainToString(dns_name, true /* require_complete */),
            absl::nullopt);
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader1, true /* require_complete */),
            absl::nullopt);

  dns_name += '\0';

  EXPECT_THAT(DnsDomainToString(dns_name, true /* require_complete */),
              testing::Optional(Eq("")));
  auto reader2 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader2, true /* require_complete */),
              testing::Optional(Eq("")));
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectCompression) {
  std::string dns_name = CreateNamePointer(152);

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);

  dns_name = "\005hello";
  dns_name += CreateNamePointer(152);

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader1), absl::nullopt);
}

// Test that extra input past the terminating zero-length label are ignored.
TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleExcessInput) {
  std::string dns_name("\004cool\004name\004test");
  dns_name += '\0';
  dns_name += "blargh!";

  EXPECT_THAT(DnsDomainToString(dns_name),
              testing::Optional(Eq("cool.name.test")));
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader),
              testing::Optional(Eq("cool.name.test")));

  dns_name = "\002hi";
  dns_name += '\0';
  dns_name += "goodbye";

  EXPECT_THAT(DnsDomainToString(dns_name), testing::Optional(Eq("hi")));
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_THAT(DnsDomainToString(reader1), testing::Optional(Eq("hi")));
}

// Test that input is malformed if it ends mid label.
TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectTruncatedNames) {
  std::string dns_name = "\07cheese";

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);

  dns_name = "\006cheesy\05test";

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader1 = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader1), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleLongSingleLabel) {
  std::string dns_name(1, static_cast<char>(dns_protocol::kMaxLabelLength));
  for (int i = 0; i < dns_protocol::kMaxLabelLength; ++i) {
    dns_name += 'a';
  }

  EXPECT_NE(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_NE(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleLongSecondLabel) {
  std::string dns_name("\003foo");
  dns_name += static_cast<char>(dns_protocol::kMaxLabelLength);
  for (int i = 0; i < dns_protocol::kMaxLabelLength; ++i) {
    dns_name += 'a';
  }

  EXPECT_NE(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_NE(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectTooLongSingleLabel) {
  std::string dns_name(1, static_cast<char>(dns_protocol::kMaxLabelLength));
  for (int i = 0; i < dns_protocol::kMaxLabelLength + 1; ++i) {
    dns_name += 'a';
  }

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectTooLongSecondLabel) {
  std::string dns_name("\003foo");
  dns_name += static_cast<char>(dns_protocol::kMaxLabelLength);
  for (int i = 0; i < dns_protocol::kMaxLabelLength + 1; ++i) {
    dns_name += 'a';
  }

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);
}

#if CHAR_MIN < 0
TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectCharMinLabels) {
  ASSERT_GT(static_cast<uint8_t>(CHAR_MIN), dns_protocol::kMaxLabelLength);

  std::string dns_name;
  dns_name += base::checked_cast<char>(CHAR_MIN);

  // Wherever possible, make the name otherwise valid.
  if (static_cast<uint8_t>(CHAR_MIN) < UINT8_MAX) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(CHAR_MIN); ++i) {
      dns_name += 'a';
    }
  }

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);
}
#endif  // if CHAR_MIN < 0

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleLongName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - 1 - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  ASSERT_EQ(dns_name.size(), static_cast<size_t>(dns_protocol::kMaxNameLength));

  EXPECT_NE(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_NE(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectTooLongName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength + 1;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 1));

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldHandleLongCompleteName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - 1 - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  dns_name += '\0';
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 1));

  EXPECT_NE(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_NE(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, DnsDomainToStringShouldRejectTooLongCompleteName) {
  std::string dns_name;
  for (int i = 0; i < dns_protocol::kMaxNameLength + 1;
       i += (dns_protocol::kMaxLabelLength + 1)) {
    int label_size = std::min(dns_protocol::kMaxNameLength - i,
                              dns_protocol::kMaxLabelLength);
    dns_name += static_cast<char>(label_size);
    for (int j = 0; j < label_size; ++j) {
      dns_name += 'a';
    }
  }
  dns_name += '\0';
  ASSERT_EQ(dns_name.size(),
            static_cast<size_t>(dns_protocol::kMaxNameLength + 2));

  EXPECT_EQ(DnsDomainToString(dns_name), absl::nullopt);
  auto reader = base::BigEndianReader::FromStringPiece(dns_name);
  EXPECT_EQ(DnsDomainToString(reader), absl::nullopt);
}

TEST_F(DNSUtilTest, IsValidDNSDomain) {
  const char* const bad_hostnames[] = {
      "%20%20noodles.blorg", "noo dles.blorg ",    "noo dles.blorg. ",
      "^noodles.blorg",      "noodles^.blorg",     "noo&dles.blorg",
      "noodles.blorg`",      "www.-noodles.blorg",
  };

  for (const auto* bad_hostname : bad_hostnames) {
    EXPECT_FALSE(IsValidDNSDomain(bad_hostname));
  }

  const char* const good_hostnames[] = {
      "www.noodles.blorg",   "1www.noodles.blorg", "www.2noodles.blorg",
      "www.n--oodles.blorg", "www.noodl_es.blorg", "www.no-_odles.blorg",
      "www_.noodles.blorg",  "www.noodles.blorg.", "_privet._tcp.local",
  };

  for (const auto* good_hostname : good_hostnames) {
    EXPECT_TRUE(IsValidDNSDomain(good_hostname));
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

  for (const auto* good_hostname : good_hostnames) {
    EXPECT_TRUE(IsValidUnrestrictedDNSDomain(good_hostname));
  }
}

TEST_F(DNSUtilTest, GetURLFromTemplateWithoutParameters) {
  EXPECT_EQ("https://dnsserver.example.net/dns-query",
            GetURLFromTemplateWithoutParameters(
                "https://dnsserver.example.net/dns-query{?dns}"));
}

TEST_F(DNSUtilTest, GetDohUpgradeServersFromDotHostname) {
  std::vector<DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromDotHostname("");
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname("unrecognized");
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org");
  EXPECT_EQ(1u, doh_servers.size());
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
            doh_servers[0].server_template());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          GetDohProviderEntry("CleanBrowsingFamily").feature});
  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org");
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

  nameservers.emplace_back(dns_ip0, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip1, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip2, 54);
  nameservers.emplace_back(dns_ip3, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip4, dns_protocol::kDefaultPort);

  std::vector<DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromNameservers(std::vector<IPEndPoint>());
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromNameservers(nameservers);
  auto expected_config = *DnsOverHttpsConfig::FromTemplatesForTesting(
      {"https://chrome.cloudflare-dns.com/dns-query",
       "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
       "https://doh.cleanbrowsing.org/doh/security-filter{?dns}"});
  EXPECT_EQ(expected_config.servers(), doh_servers);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{GetDohProviderEntry("CleanBrowsingSecure").feature,
                             GetDohProviderEntry("Cloudflare").feature});

  doh_servers = GetDohUpgradeServersFromNameservers(nameservers);
  EXPECT_THAT(doh_servers,
              testing::ElementsAre(*DnsOverHttpsServerConfig::FromString(
                  "https://doh.cleanbrowsing.org/doh/family-filter{?dns}")));
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromServerConfig) {
  EXPECT_EQ("Cloudflare",
            GetDohProviderIdForHistogramFromServerConfig(
                *DnsOverHttpsServerConfig::FromString(
                    "https://chrome.cloudflare-dns.com/dns-query")));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromServerConfig(
                         *DnsOverHttpsServerConfig::FromString(
                             "https://unexpected.dohserver.com/dns-query")));
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromNameserver) {
  EXPECT_EQ("CleanBrowsingSecure",
            GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                IPAddress(185, 228, 169, 9), dns_protocol::kDefaultPort)));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                         IPAddress(1, 2, 3, 4), dns_protocol::kDefaultPort)));
}

}  // namespace net
