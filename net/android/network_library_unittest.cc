// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_library.h"

#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace android {

TEST(NetworkLibraryTest, CaptivePortal) {
  EXPECT_FALSE(android::GetIsCaptivePortal());
}

TEST(NetworkLibraryTest, GetWifiSignalLevel) {
  base::Optional<int32_t> signal_strength = android::GetWifiSignalLevel();
  if (!signal_strength.has_value())
    return;
  EXPECT_LE(0, signal_strength.value());
  EXPECT_GE(4, signal_strength.value());
}

TEST(NetworkLibraryTest, GetDnsSearchDomains) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    GTEST_SKIP() << "Cannot call or test GetDnsServers() in pre-M.";
  }

  std::vector<IPEndPoint> dns_servers;
  bool dns_over_tls_active;
  std::string dns_over_tls_hostname;
  std::vector<std::string> search_suffixes;

  if (!GetDnsServers(&dns_servers, &dns_over_tls_active, &dns_over_tls_hostname,
                     &search_suffixes)) {
    return;
  }

  for (std::string suffix : search_suffixes) {
    EXPECT_FALSE(suffix.empty());
  }
}

}  // namespace android

}  // namespace net
