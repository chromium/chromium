// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces.h"

#include <ostream>
#include <string>
#include <unordered_set>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX) && !defined(OS_ANDROID)
#include <net/if.h>
#elif defined(OS_WIN)
#include <iphlpapi.h>
#include <objbase.h>
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#endif

namespace net {

namespace {

// Verify GetNetworkList().
TEST(NetworkInterfacesTest, GetNetworkList) {
  NetworkInterfaceList list;
  ASSERT_TRUE(GetNetworkList(&list, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES));
  for (auto it = list.begin(); it != list.end(); ++it) {
    // Verify that the names are not empty.
    EXPECT_FALSE(it->name.empty());
    EXPECT_FALSE(it->friendly_name.empty());

    // Verify that the address is correct.
    EXPECT_TRUE(it->address.IsValid()) << "Invalid address of size "
                                       << it->address.size();
    EXPECT_FALSE(it->address.IsZero());
    EXPECT_GT(it->prefix_length, 1u);
    EXPECT_LE(it->prefix_length, it->address.size() * 8);

#if defined(OS_WIN)
    // On Windows |name| is NET_LUID.
    NET_LUID luid;
    EXPECT_EQ(static_cast<DWORD>(NO_ERROR),
              ConvertInterfaceIndexToLuid(it->interface_index, &luid));
    GUID guid;
    EXPECT_EQ(static_cast<DWORD>(NO_ERROR),
              ConvertInterfaceLuidToGuid(&luid, &guid));
    auto name = base::win::String16FromGUID(guid);
    EXPECT_EQ(base::as_u16cstr(base::UTF8ToWide(it->name)), name);

    if (it->type == NetworkChangeNotifier::CONNECTION_WIFI) {
      EXPECT_NE(WIFI_PHY_LAYER_PROTOCOL_NONE, GetWifiPHYLayerProtocol());
    }
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
    char name[IF_NAMESIZE];
    EXPECT_TRUE(if_indextoname(it->interface_index, name));
    EXPECT_STREQ(it->name.c_str(), name);
#endif
  }
}

TEST(NetworkInterfacesTest, GetWifiSSID) {
  // We can't check the result of GetWifiSSID() directly, since the result
  // will differ across machines. Simply exercise the code path and hope that it
  // doesn't crash.
  EXPECT_NE((const char*)nullptr, GetWifiSSID().c_str());
}

TEST(NetworkInterfacesTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = GetHostName();
  EXPECT_FALSE(hostname.empty());
}

}  // namespace

}  // namespace net
