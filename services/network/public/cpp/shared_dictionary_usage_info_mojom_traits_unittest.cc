// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_dictionary_usage_info_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/shared_dictionary_usage_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

TEST(SharedDictionaryUsageInfoMojomTraitsTest, SerializeAndDeserialize) {
  net::SharedDictionaryUsageInfo original = net::SharedDictionaryUsageInfo{
      .isolation_key = net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL("https://a.test")),
          net::SchemefulSite(url::Origin::Create(GURL("https://b.test")))),
      .total_size_bytes = 1234};
  net::SharedDictionaryUsageInfo copied;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::SharedDictionaryUsageInfo>(
          original, copied));
  EXPECT_EQ(original, copied);
}

TEST(SharedDictionaryUsageInfoMojomTraitsTest, OpaqueIsolationKey) {
  mojom::SharedDictionaryUsageInfoPtr usage_info =
      mojom::SharedDictionaryUsageInfo::New();
  usage_info->isolation_key = net::SharedDictionaryIsolationKey();
  usage_info->total_size_bytes = 2345;

  std::vector<uint8_t> serialized =
      mojom::SharedDictionaryUsageInfo::Serialize(&usage_info);
  net::SharedDictionaryUsageInfo deserialized;
  EXPECT_FALSE(
      mojom::SharedDictionaryUsageInfo::Deserialize(serialized, &deserialized));
}

}  // namespace
}  // namespace network
