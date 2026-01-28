// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/win_dns_system_settings.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "net/dns/dns_test_util.h"
#endif

namespace net {

namespace {


TEST(WinDnsSystemSettings, GetAllNameServersEmpty) {
  const std::vector<AdapterInfo> infos = {{
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
                                          }};

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  std::optional<std::vector<IPEndPoint>> nameservers =
      settings.GetAllNameservers();
  EXPECT_TRUE(nameservers.has_value());
  EXPECT_TRUE(nameservers.value().empty());
}

TEST(WinDnsSystemSettings, GetAllNameServersStatelessDiscoveryAdresses) {
  const std::vector<AdapterInfo> infos = {
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
      }};

  WinDnsSystemSettings settings;
  settings.addresses = CreateAdapterAddresses(infos);
  std::optional<std::vector<IPEndPoint>> nameservers =
      settings.GetAllNameservers();
  EXPECT_TRUE(nameservers.has_value());
  EXPECT_TRUE(nameservers.value().empty());
}

TEST(WinDnsSystemSettings, GetAllNameServersValid) {
  const std::vector<AdapterInfo> infos = {
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
       .ports = {33, 44}}};

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
