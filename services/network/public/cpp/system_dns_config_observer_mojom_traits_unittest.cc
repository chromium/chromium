// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/system_dns_config_observer_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/system_dns_config_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(SystemDnsConfigObserverMojomTraitsTest,
     SerializeAndDeserializeDefaultValue) {
  net::DnsConfig original, deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfig>(
      original, deserialized));

  EXPECT_EQ(original, deserialized) << "original=" << original.ToDict()
                                    << "deserialized=" << deserialized.ToDict();
}

TEST(SystemDnsConfigObserverMojomTraitsTest, SerializeAndDeserializeWithValue) {
  net::DnsConfig original;
  original.nameservers = {net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80)};
  original.dns_over_tls_active = true;
  original.dns_over_tls_hostname = "https://example.com/";
  original.search = {"foo"};
  original.unhandled_options = true;

  net::DnsConfig deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfig>(
      original, deserialized));

  EXPECT_EQ(original, deserialized) << "original=" << original.ToDict()
                                    << "deserialized=" << deserialized.ToDict();
}

}  // namespace
}  // namespace network
