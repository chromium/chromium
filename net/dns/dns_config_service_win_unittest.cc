// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_config_service_win.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/free_deleter.h"
#include "base/test/gmock_expected_support.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/win_dns_system_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsConfigServiceWinTest, ParseSearchList) {
  const struct TestCase {
    const wchar_t* input;
    std::vector<std::string> expected;
  } cases[] = {
      {L"chromium.org", {"chromium.org"}},
      {L"chromium.org,org", {"chromium.org", "org"}},
      // Empty suffixes terminate the list
      {L"crbug.com,com,,org", {"crbug.com", "com"}},
      // IDN are converted to punycode
      {L"\u017c\xf3\u0142ta.pi\u0119\u015b\u0107.pl,pl",
       {"xn--ta-4ja03asj.xn--pi-wla5e0q.pl", "pl"}},
      // Empty search list is invalid
      {L"", {}},
      {L",,", {}},
  };

  for (const auto& t : cases) {
    EXPECT_EQ(internal::ParseSearchList(t.input), t.expected);
  }
}

struct AdapterInfo {
  IFTYPE if_type;
  IF_OPER_STATUS oper_status;
  const WCHAR* dns_suffix;
  std::string dns_server_addresses[4];  // Empty string indicates end.
  uint16_t ports[4];
};

std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> CreateAdapterAddresses(
    const AdapterInfo* infos) {
  size_t num_adapters = 0;
  size_t num_addresses = 0;
  for (size_t i = 0; infos[i].if_type; ++i) {
    ++num_adapters;
    for (size_t j = 0; !infos[i].dns_server_addresses[j].empty(); ++j) {
      ++num_addresses;
    }
  }

  size_t heap_size = num_adapters * sizeof(IP_ADAPTER_ADDRESSES) +
                     num_addresses * (sizeof(IP_ADAPTER_DNS_SERVER_ADDRESS) +
                                      sizeof(struct sockaddr_storage));
  std::unique_ptr<IP_ADAPTER_ADDRESSES, base::FreeDeleter> heap(
      static_cast<IP_ADAPTER_ADDRESSES*>(malloc(heap_size)));
  CHECK(heap.get());
  memset(heap.get(), 0, heap_size);

  IP_ADAPTER_ADDRESSES* adapters = heap.get();
  IP_ADAPTER_DNS_SERVER_ADDRESS* addresses =
      reinterpret_cast<IP_ADAPTER_DNS_SERVER_ADDRESS*>(adapters + num_adapters);
  struct sockaddr_storage* storage =
      reinterpret_cast<struct sockaddr_storage*>(addresses + num_addresses);

  for (size_t i = 0; i < num_adapters; ++i) {
    const AdapterInfo& info = infos[i];
    IP_ADAPTER_ADDRESSES* adapter = adapters + i;
    if (i + 1 < num_adapters)
      adapter->Next = adapter + 1;
    adapter->IfType = info.if_type;
    adapter->OperStatus = info.oper_status;
    adapter->DnsSuffix = const_cast<PWCHAR>(info.dns_suffix);
    IP_ADAPTER_DNS_SERVER_ADDRESS* address = nullptr;
    for (size_t j = 0; !info.dns_server_addresses[j].empty(); ++j) {
      --num_addresses;
      if (j == 0) {
        address = adapter->FirstDnsServerAddress = addresses + num_addresses;
      } else {
        // Note that |address| is moving backwards.
        address = address->Next = address - 1;
      }
      IPAddress ip;
      CHECK(ip.AssignFromIPLiteral(info.dns_server_addresses[j]));
      IPEndPoint ipe = IPEndPoint(ip, info.ports[j]);
      address->Address.lpSockaddr =
          reinterpret_cast<LPSOCKADDR>(storage + num_addresses);
      socklen_t length = sizeof(struct sockaddr_storage);
      CHECK(ipe.ToSockAddr(address->Address.lpSockaddr, &length));
      address->Address.iSockaddrLength = static_cast<int>(length);
    }
  }

  return heap;
}

TEST(DnsConfigServiceWinTest, ConvertAdapterAddresses) {
  // Check nameservers and connection-specific suffix.
  const struct TestCase {
    AdapterInfo input_adapters[4];        // |if_type| == 0 indicates end.
    std::string expected_nameservers[4];  // Empty string indicates end.
    std::string expected_suffix;
    uint16_t expected_ports[4];
  } cases[] = {
    {  // Ignore loopback and inactive adapters.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"funnyloop",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusDormant, L"example.com",
          { "1.0.0.1" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
          { "10.0.0.10", "2001:FFFF::1111" } },
        { 0 },
      },
      { "10.0.0.10", "2001:FFFF::1111" },
      "chromium.org",
    },
    {  // Respect configured ports.
      {
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
        { "10.0.0.10", "2001:FFFF::1111" }, { 1024, 24 } },
        { 0 },
      },
      { "10.0.0.10", "2001:FFFF::1111" },
      "chromium.org",
      { 1024, 24 },
    },
    {  // Use the preferred adapter (first in binding order) and filter
       // stateless DNS discovery addresses.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"funnyloop",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusUp, L"example.com",
          { "1.0.0.1", "fec0:0:0:ffff::2", "8.8.8.8" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org",
          { "10.0.0.10", "2001:FFFF::1111" } },
        { 0 },
      },
      { "1.0.0.1", "8.8.8.8" },
      "example.com",
    },
    {  // No usable adapters.
      {
        { IF_TYPE_SOFTWARE_LOOPBACK, IfOperStatusUp, L"localhost",
          { "2.0.0.2" } },
        { IF_TYPE_FASTETHER, IfOperStatusDormant, L"example.com",
          { "1.0.0.1" } },
        { IF_TYPE_USB, IfOperStatusUp, L"chromium.org" },
        { 0 },
      },
    },
  };

  for (const auto& t : cases) {
    WinDnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(t.input_adapters);
    // Default settings for the rest.
    std::vector<IPEndPoint> expected_nameservers;
    for (size_t j = 0; !t.expected_nameservers[j].empty(); ++j) {
      IPAddress ip;
      ASSERT_TRUE(ip.AssignFromIPLiteral(t.expected_nameservers[j]));
      uint16_t port = t.expected_ports[j];
      if (!port)
        port = dns_protocol::kDefaultPort;
      expected_nameservers.push_back(IPEndPoint(ip, port));
    }

    base::expected<DnsConfig, ReadWinSystemDnsSettingsError> config_or_error =
        internal::ConvertSettingsToDnsConfig(std::move(settings));
    bool expected_success = !expected_nameservers.empty();
    EXPECT_EQ(expected_success, config_or_error.has_value());
    if (config_or_error.has_value()) {
      EXPECT_EQ(expected_nameservers, config_or_error->nameservers);
      EXPECT_THAT(config_or_error->search,
                  testing::ElementsAre(t.expected_suffix));
    }
  }
}

TEST(DnsConfigServiceWinTest, ConvertSuffixSearch) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    struct {
      std::optional<std::wstring> policy_search_list;
      std::optional<std::wstring> tcpip_search_list;
      std::optional<std::wstring> tcpip_domain;
      std::optional<std::wstring> primary_dns_suffix;
      WinDnsSystemSettings::DevolutionSetting policy_devolution;
      WinDnsSystemSettings::DevolutionSetting dnscache_devolution;
      WinDnsSystemSettings::DevolutionSetting tcpip_devolution;
    } input_settings;
    std::vector<std::string> expected_search;
  } cases[] = {
      {
          // Policy SearchList override.
          {
              L"policy.searchlist.a,policy.searchlist.b",
              L"tcpip.searchlist.a,tcpip.searchlist.b",
              L"tcpip.domain",
              L"primary.dns.suffix",
          },
          {"policy.searchlist.a", "policy.searchlist.b"},
      },
      {
          // User-specified SearchList override.
          {
              std::nullopt,
              L"tcpip.searchlist.a,tcpip.searchlist.b",
              L"tcpip.domain",
              L"primary.dns.suffix",
          },
          {"tcpip.searchlist.a", "tcpip.searchlist.b"},
      },
      {
          // Void SearchList. Using tcpip.domain
          {
              L",bad.searchlist,parsed.as.empty",
              L"tcpip.searchlist,good.but.overridden",
              L"tcpip.domain",
              std::nullopt,
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // Void SearchList. Using primary.dns.suffix
          {
              L",bad.searchlist,parsed.as.empty",
              L"tcpip.searchlist,good.but.overridden",
              L"tcpip.domain",
              L"primary.dns.suffix",
          },
          {"primary.dns.suffix", "connection.suffix"},
      },
      {
          // Void SearchList. Using tcpip.domain when primary.dns.suffix is
          // empty
          {
              L",bad.searchlist,parsed.as.empty",
              L"tcpip.searchlist,good.but.overridden",
              L"tcpip.domain",
              L"",
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // Void SearchList. Using tcpip.domain when primary.dns.suffix is NULL
          {
              L",bad.searchlist,parsed.as.empty",
              L"tcpip.searchlist,good.but.overridden",
              L"tcpip.domain",
              L"",
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // No primary suffix. Devolution does not matter.
          {
              std::nullopt,
              std::nullopt,
              L"",
              L"",
              {1, 2},
          },
          {"connection.suffix"},
      },
      {
          // Devolution enabled by policy, level by dnscache.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              std::nullopt,
              {1, std::nullopt},  // policy_devolution: enabled, level
              {0, 3},             // dnscache_devolution
              {0, 1},             // tcpip_devolution
          },
          {"a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e"},
      },
      {
          // Devolution enabled by dnscache, level by policy.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              L"f.g.i.l.j",
              {std::nullopt, 4},
              {1, std::nullopt},
              {0, 3},
          },
          {"f.g.i.l.j", "connection.suffix", "g.i.l.j"},
      },
      {
          // Devolution enabled by default.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              std::nullopt,
              {std::nullopt, std::nullopt},
              {std::nullopt, 3},
              {std::nullopt, 1},
          },
          {"a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e"},
      },
      {
          // Devolution enabled at level = 2, but nothing to devolve.
          {
              std::nullopt,
              std::nullopt,
              L"a.b",
              std::nullopt,
              {std::nullopt, std::nullopt},
              {std::nullopt, 2},
              {std::nullopt, 2},
          },
          {"a.b", "connection.suffix"},
      },
      {
          // Devolution disabled when no explicit level.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              std::nullopt,
              {1, std::nullopt},
              {1, std::nullopt},
              {1, std::nullopt},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
      {
          // Devolution disabled by policy level.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              std::nullopt,
              {std::nullopt, 1},
              {1, 3},
              {1, 4},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
      {
          // Devolution disabled by user setting.
          {
              std::nullopt,
              std::nullopt,
              L"a.b.c.d.e",
              std::nullopt,
              {std::nullopt, 3},
              {std::nullopt, 3},
              {0, 3},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
  };

  for (auto& t : cases) {
    WinDnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.policy_search_list = t.input_settings.policy_search_list;
    settings.tcpip_search_list = t.input_settings.tcpip_search_list;
    settings.tcpip_domain = t.input_settings.tcpip_domain;
    settings.primary_dns_suffix = t.input_settings.primary_dns_suffix;
    settings.policy_devolution = t.input_settings.policy_devolution;
    settings.dnscache_devolution = t.input_settings.dnscache_devolution;
    settings.tcpip_devolution = t.input_settings.tcpip_devolution;

    ASSERT_OK_AND_ASSIGN(
        DnsConfig dns_config,
        internal::ConvertSettingsToDnsConfig(std::move(settings)));
    EXPECT_THAT(dns_config,
                testing::Field(&DnsConfig::search,
                               testing::ElementsAreArray(t.expected_search)));
  }
}

TEST(DnsConfigServiceWinTest, AppendToMultiLabelName) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    std::optional<DWORD> input;
    bool expected_output;
  } cases[] = {
      {0, false},
      {1, true},
      {std::nullopt, false},
  };

  for (const auto& t : cases) {
    WinDnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.append_to_multi_label_name = t.input;
    ASSERT_OK_AND_ASSIGN(
        DnsConfig dns_config,
        internal::ConvertSettingsToDnsConfig(std::move(settings)));
    EXPECT_THAT(dns_config,
                testing::Field(&DnsConfig::append_to_multi_label_name,
                               testing::Eq(t.expected_output)));
  }
}

// Setting have_name_resolution_policy_table should set `unhandled_options`.
TEST(DnsConfigServiceWinTest, HaveNRPT) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    bool have_nrpt;
    bool unhandled_options;
  } cases[] = {
      {false, false},
      {true, true},
  };

  for (const auto& t : cases) {
    WinDnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.have_name_resolution_policy = t.have_nrpt;
    ASSERT_OK_AND_ASSIGN(
        DnsConfig dns_config,
        internal::ConvertSettingsToDnsConfig(std::move(settings)));
    EXPECT_EQ(t.unhandled_options, dns_config.unhandled_options);
    EXPECT_EQ(t.have_nrpt, dns_config.use_local_ipv6);
  }
}

// Setting have_proxy should set `unhandled_options`.
TEST(DnsConfigServiceWinTest, HaveProxy) {
  AdapterInfo infos[2] = {
      {IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", {"1.0.0.1"}},
      {0},
  };

  const struct TestCase {
    bool have_proxy;
    bool unhandled_options;
  } cases[] = {
      {false, false},
      {true, true},
  };

  for (const auto& t : cases) {
    WinDnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.have_proxy = t.have_proxy;
    ASSERT_OK_AND_ASSIGN(
        DnsConfig dns_config,
        internal::ConvertSettingsToDnsConfig(std::move(settings)));
    EXPECT_THAT(dns_config, testing::Field(&DnsConfig::unhandled_options,
                                           testing::Eq(t.unhandled_options)));
  }
}

// Setting uses_vpn should set `unhandled_options`.
TEST(DnsConfigServiceWinTest, UsesVpn) {
  AdapterInfo infos[3] = {
      {IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", {"1.0.0.1"}},
      {IF_TYPE_PPP, IfOperStatusUp, L"connection.suffix", {"1.0.0.1"}},
      {0},
  };

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  ASSERT_OK_AND_ASSIGN(
      DnsConfig dns_config,
      internal::ConvertSettingsToDnsConfig(std::move(settings)));
  EXPECT_THAT(dns_config,
              testing::Field(&DnsConfig::unhandled_options, testing::IsTrue()));
}

// Setting adapter specific nameservers should set `unhandled_options`.
TEST(DnsConfigServiceWinTest, AdapterSpecificNameservers) {
  AdapterInfo infos[3] = {
      {IF_TYPE_FASTETHER,
       IfOperStatusUp,
       L"example.com",
       {"1.0.0.1", "fec0:0:0:ffff::2", "8.8.8.8"}},
      {IF_TYPE_USB,
       IfOperStatusUp,
       L"chromium.org",
       {"10.0.0.10", "2001:FFFF::1111"}},
      {0},
  };

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  ASSERT_OK_AND_ASSIGN(
      DnsConfig dns_config,
      internal::ConvertSettingsToDnsConfig(std::move(settings)));
  EXPECT_THAT(dns_config,
              testing::Field(&DnsConfig::unhandled_options, testing::IsTrue()));
}

// Setting adapter specific nameservers for non operational adapter should not
// set `unhandled_options`.
TEST(DnsConfigServiceWinTest, AdapterSpecificNameserversForNo) {
  AdapterInfo infos[3] = {
      {IF_TYPE_FASTETHER,
       IfOperStatusUp,
       L"example.com",
       {"1.0.0.1", "fec0:0:0:ffff::2", "8.8.8.8"}},
      {IF_TYPE_USB,
       IfOperStatusDown,
       L"chromium.org",
       {"10.0.0.10", "2001:FFFF::1111"}},
      {0},
  };

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  ASSERT_OK_AND_ASSIGN(
      DnsConfig dns_config,
      internal::ConvertSettingsToDnsConfig(std::move(settings)));
  EXPECT_THAT(dns_config, testing::Field(&DnsConfig::unhandled_options,
                                         testing::IsFalse()));
}

}  // namespace

}  // namespace net
