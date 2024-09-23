// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/public/win_dns_system_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

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

TEST(WinDnsSystemSettings, GetAllNameServersEmpty) {
  AdapterInfo infos[3] = {
      {
          .if_type = IF_TYPE_USB,
          .oper_status = IfOperStatusUp,
          .dns_suffix = L"example.com",
          .dns_server_addresses = {},
      },
      {
          .if_type = IF_TYPE_USB,
          .oper_status = IfOperStatusUp,
          .dns_suffix = L"foo.bar",
          .dns_server_addresses = {},
      },
      {0}};

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  std::optional<std::vector<IPEndPoint>> nameservers =
      settings.GetAllNameservers();
  EXPECT_TRUE(nameservers.has_value());
  EXPECT_TRUE(nameservers.value().empty());
}

TEST(WinDnsSystemSettings, GetAllNameServersStatelessDiscoveryAdresses) {
  AdapterInfo infos[3] = {
      {
          .if_type = IF_TYPE_USB,
          .oper_status = IfOperStatusUp,
          .dns_suffix = L"example.com",
          .dns_server_addresses = {"fec0:0:0:ffff::1", "fec0:0:0:ffff::2"},
      },
      {
          .if_type = IF_TYPE_USB,
          .oper_status = IfOperStatusUp,
          .dns_suffix = L"foo.bar",
          .dns_server_addresses = {"fec0:0:0:ffff::3"},
      },
      {0}};

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  std::optional<std::vector<IPEndPoint>> nameservers =
      settings.GetAllNameservers();
  EXPECT_TRUE(nameservers.has_value());
  EXPECT_TRUE(nameservers.value().empty());
}

TEST(WinDnsSystemSettings, GetAllNameServersValid) {
  AdapterInfo infos[3] = {
      {.if_type = IF_TYPE_USB,
       .oper_status = IfOperStatusUp,
       .dns_suffix = L"example.com",
       .dns_server_addresses = {"8.8.8.8", "10.0.0.10"},
       .ports = {11, 22}},
      {.if_type = IF_TYPE_USB,
       .oper_status = IfOperStatusUp,
       .dns_suffix = L"foo.bar",
       .dns_server_addresses = {"2001:ffff::1111",
                                "aaaa:bbbb:cccc:dddd:eeee:ffff:0:1"},
       .ports = {33, 44}},
      {0}};

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  std::optional<std::vector<IPEndPoint>> nameservers =
      settings.GetAllNameservers();
  EXPECT_TRUE(nameservers.has_value());
  EXPECT_EQ(4u, nameservers.value().size());
  EXPECT_EQ(nameservers.value()[0].ToString(), "8.8.8.8:11");
  EXPECT_EQ(nameservers.value()[1].ToString(), "10.0.0.10:22");
  EXPECT_EQ(nameservers.value()[2].ToString(), "[2001:ffff::1111]:33");
  EXPECT_EQ(nameservers.value()[3].ToString(),
            "[aaaa:bbbb:cccc:dddd:eeee:ffff:0:1]:44");
}
}  // namespace

}  // namespace net
