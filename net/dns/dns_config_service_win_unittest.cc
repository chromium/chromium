// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsConfigServiceWinTest, ParseSearchList) {
  const struct TestCase {
    const base::char16* input;
    const char* output[4];  // NULL-terminated, empty if expected false
  } cases[] = {
      {STRING16_LITERAL("chromium.org"), {"chromium.org", nullptr}},
      {STRING16_LITERAL("chromium.org,org"), {"chromium.org", "org", nullptr}},
      // Empty suffixes terminate the list
      {STRING16_LITERAL("crbug.com,com,,org"), {"crbug.com", "com", nullptr}},
      // IDN are converted to punycode
      {STRING16_LITERAL("\u017c\xf3\u0142ta.pi\u0119\u015b\u0107.pl,pl"),
       {"xn--ta-4ja03asj.xn--pi-wla5e0q.pl", "pl", nullptr}},
      // Empty search list is invalid
      {STRING16_LITERAL(""), {nullptr}},
      {STRING16_LITERAL(",,"), {nullptr}},
  };

  for (const auto& t : cases) {
    std::vector<std::string> actual_output, expected_output;
    actual_output.push_back("UNSET");
    for (const char* const* output = t.output; *output; ++output) {
      expected_output.push_back(*output);
    }
    bool result = internal::ParseSearchList(t.input, &actual_output);
    if (!expected_output.empty()) {
      EXPECT_TRUE(result);
      EXPECT_EQ(expected_output, actual_output);
    } else {
      EXPECT_FALSE(result) << "Unexpected parse success on " << t.input;
    }
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
    internal::DnsSystemSettings settings;
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

    DnsConfig config;
    internal::ConfigParseWinResult result =
        internal::ConvertSettingsToDnsConfig(settings, &config);
    internal::ConfigParseWinResult expected_result =
        expected_nameservers.empty() ? internal::CONFIG_PARSE_WIN_NO_NAMESERVERS
            : internal::CONFIG_PARSE_WIN_OK;
    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(expected_nameservers, config.nameservers);
    if (result == internal::CONFIG_PARSE_WIN_OK) {
      ASSERT_EQ(1u, config.search.size());
      EXPECT_EQ(t.expected_suffix, config.search[0]);
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
      internal::DnsSystemSettings::RegString policy_search_list;
      internal::DnsSystemSettings::RegString tcpip_search_list;
      internal::DnsSystemSettings::RegString tcpip_domain;
      internal::DnsSystemSettings::RegString primary_dns_suffix;
      internal::DnsSystemSettings::DevolutionSetting policy_devolution;
      internal::DnsSystemSettings::DevolutionSetting dnscache_devolution;
      internal::DnsSystemSettings::DevolutionSetting tcpip_devolution;
    } input_settings;
    std::string expected_search[5];
  } cases[] = {
      {
          // Policy SearchList override.
          {
              {true,
               STRING16_LITERAL("policy.searchlist.a,policy.searchlist.b")},
              {true, STRING16_LITERAL("tcpip.searchlist.a,tcpip.searchlist.b")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {true, STRING16_LITERAL("primary.dns.suffix")},
          },
          {"policy.searchlist.a", "policy.searchlist.b"},
      },
      {
          // User-specified SearchList override.
          {
              {false},
              {true, STRING16_LITERAL("tcpip.searchlist.a,tcpip.searchlist.b")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {true, STRING16_LITERAL("primary.dns.suffix")},
          },
          {"tcpip.searchlist.a", "tcpip.searchlist.b"},
      },
      {
          // Void SearchList. Using tcpip.domain
          {
              {true, STRING16_LITERAL(",bad.searchlist,parsed.as.empty")},
              {true, STRING16_LITERAL("tcpip.searchlist,good.but.overridden")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {false},
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // Void SearchList. Using primary.dns.suffix
          {
              {true, STRING16_LITERAL(",bad.searchlist,parsed.as.empty")},
              {true, STRING16_LITERAL("tcpip.searchlist,good.but.overridden")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {true, STRING16_LITERAL("primary.dns.suffix")},
          },
          {"primary.dns.suffix", "connection.suffix"},
      },
      {
          // Void SearchList. Using tcpip.domain when primary.dns.suffix is
          // empty
          {
              {true, STRING16_LITERAL(",bad.searchlist,parsed.as.empty")},
              {true, STRING16_LITERAL("tcpip.searchlist,good.but.overridden")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {true, STRING16_LITERAL("")},
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // Void SearchList. Using tcpip.domain when primary.dns.suffix is NULL
          {
              {true, STRING16_LITERAL(",bad.searchlist,parsed.as.empty")},
              {true, STRING16_LITERAL("tcpip.searchlist,good.but.overridden")},
              {true, STRING16_LITERAL("tcpip.domain")},
              {true},
          },
          {"tcpip.domain", "connection.suffix"},
      },
      {
          // No primary suffix. Devolution does not matter.
          {
              {false},
              {false},
              {true},
              {true},
              {{true, 1}, {true, 2}},
          },
          {"connection.suffix"},
      },
      {
          // Devolution enabled by policy, level by dnscache.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {false},
              {{true, 1}, {false}},    // policy_devolution: enabled, level
              {{true, 0}, {true, 3}},  // dnscache_devolution
              {{true, 0}, {true, 1}},  // tcpip_devolution
          },
          {"a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e"},
      },
      {
          // Devolution enabled by dnscache, level by policy.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {true, STRING16_LITERAL("f.g.i.l.j")},
              {{false}, {true, 4}},
              {{true, 1}, {false}},
              {{true, 0}, {true, 3}},
          },
          {"f.g.i.l.j", "connection.suffix", "g.i.l.j"},
      },
      {
          // Devolution enabled by default.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {false},
              {{false}, {false}},
              {{false}, {true, 3}},
              {{false}, {true, 1}},
          },
          {"a.b.c.d.e", "connection.suffix", "b.c.d.e", "c.d.e"},
      },
      {
          // Devolution enabled at level = 2, but nothing to devolve.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b")},
              {false},
              {{false}, {false}},
              {{false}, {true, 2}},
              {{false}, {true, 2}},
          },
          {"a.b", "connection.suffix"},
      },
      {
          // Devolution disabled when no explicit level.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {false},
              {{true, 1}, {false}},
              {{true, 1}, {false}},
              {{true, 1}, {false}},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
      {
          // Devolution disabled by policy level.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {false},
              {{false}, {true, 1}},
              {{true, 1}, {true, 3}},
              {{true, 1}, {true, 4}},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
      {
          // Devolution disabled by user setting.
          {
              {false},
              {false},
              {true, STRING16_LITERAL("a.b.c.d.e")},
              {false},
              {{false}, {true, 3}},
              {{false}, {true, 3}},
              {{true, 0}, {true, 3}},
          },
          {"a.b.c.d.e", "connection.suffix"},
      },
  };

  for (auto& t : cases) {
    internal::DnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.policy_search_list = t.input_settings.policy_search_list;
    settings.tcpip_search_list = t.input_settings.tcpip_search_list;
    settings.tcpip_domain = t.input_settings.tcpip_domain;
    settings.primary_dns_suffix = t.input_settings.primary_dns_suffix;
    settings.policy_devolution = t.input_settings.policy_devolution;
    settings.dnscache_devolution = t.input_settings.dnscache_devolution;
    settings.tcpip_devolution = t.input_settings.tcpip_devolution;

    DnsConfig config;
    EXPECT_EQ(internal::CONFIG_PARSE_WIN_OK,
              internal::ConvertSettingsToDnsConfig(settings, &config));
    std::vector<std::string> expected_search;
    for (size_t j = 0; !t.expected_search[j].empty(); ++j) {
      expected_search.push_back(t.expected_search[j]);
    }
    EXPECT_EQ(expected_search, config.search);
  }
}

TEST(DnsConfigServiceWinTest, AppendToMultiLabelName) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    internal::DnsSystemSettings::RegDword input;
    bool expected_output;
  } cases[] = {
      {{true, 0}, false}, {{true, 1}, true}, {{false, 0}, false},
  };

  for (const auto& t : cases) {
    internal::DnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.append_to_multi_label_name = t.input;
    DnsConfig config;
    EXPECT_EQ(internal::CONFIG_PARSE_WIN_OK,
              internal::ConvertSettingsToDnsConfig(settings, &config));
    EXPECT_EQ(t.expected_output, config.append_to_multi_label_name);
  }
}

// Setting have_name_resolution_policy_table should set unhandled_options.
TEST(DnsConfigServiceWinTest, HaveNRPT) {
  AdapterInfo infos[2] = {
    { IF_TYPE_USB, IfOperStatusUp, L"connection.suffix", { "1.0.0.1" } },
    { 0 },
  };

  const struct TestCase {
    bool have_nrpt;
    bool unhandled_options;
    internal::ConfigParseWinResult result;
  } cases[] = {
    { false, false, internal::CONFIG_PARSE_WIN_OK },
    { true, true, internal::CONFIG_PARSE_WIN_UNHANDLED_OPTIONS },
  };

  for (const auto& t : cases) {
    internal::DnsSystemSettings settings;
    settings.addresses = CreateAdapterAddresses(infos);
    settings.have_name_resolution_policy = t.have_nrpt;
    DnsConfig config;
    EXPECT_EQ(t.result,
              internal::ConvertSettingsToDnsConfig(settings, &config));
    EXPECT_EQ(t.unhandled_options, config.unhandled_options);
    EXPECT_EQ(t.have_nrpt, config.use_local_ipv6);
  }
}


}  // namespace

}  // namespace net
