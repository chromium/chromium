// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "net/cookies/cookie_partition_key.h"
#include "url/origin.h"

namespace net {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  std::string url_str = data_provider.ConsumeRandomLengthString(800);
  GURL url(url_str);

  bool has_cross_site_ancestor = data_provider.ConsumeBool();
  CookiePartitionKey::AncestorChainBit ancestor_chain_bit =
      has_cross_site_ancestor ? CookiePartitionKey::AncestorChainBit::kCrossSite
                              : CookiePartitionKey::AncestorChainBit::kSameSite;

  // Unlike FromURLForTesting and FromUntrustedInput, FromStorage requires the
  // top_level_site string passed in be formatted exactly as a SchemefulSite
  // would serialize it. Unlike FromURLForTesting, FromUntrustedInput and
  // FromStorage require the top_level_site not be opaque.
  base::expected<std::optional<CookiePartitionKey>, std::string>
      partition_key_from_string_strict =
          CookiePartitionKey::FromStorage(url_str, has_cross_site_ancestor);
  base::expected<CookiePartitionKey, std::string>
      partition_key_from_string_loose = CookiePartitionKey::FromUntrustedInput(
          url_str, has_cross_site_ancestor);
  CookiePartitionKey partition_key_from_url =
      CookiePartitionKey::FromURLForTesting(url, ancestor_chain_bit);

  if (partition_key_from_string_strict.has_value() &&
      partition_key_from_string_strict.value().has_value()) {
    // If we can deserialize from string while being strict the three keys
    // should be identical.
    CHECK_EQ(**partition_key_from_string_strict, partition_key_from_url);
    CHECK_EQ(**partition_key_from_string_strict,
             *partition_key_from_string_loose);
    // This implies we can re-serialize.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(**partition_key_from_string_strict);
    CHECK(serialized_partition_key.has_value());
    // The serialization should match the initial values.
    CHECK_EQ(serialized_partition_key->TopLevelSite(), url_str);
    CHECK_EQ(serialized_partition_key->has_cross_site_ancestor(),
             has_cross_site_ancestor);
  } else if (partition_key_from_string_loose.has_value()) {
    // If we can deserialize from string while being loose then two keys
    // should be identical.
    CHECK_EQ(*partition_key_from_string_loose, partition_key_from_url);
    // This implies we can re-serialize.
    base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                   std::string>
        serialized_partition_key =
            CookiePartitionKey::Serialize(*partition_key_from_string_loose);
    // The serialization should match the initial values.
    SchemefulSite schemeful_site(url);
    CHECK_EQ(serialized_partition_key->TopLevelSite(),
             schemeful_site.GetURL().SchemeIsFile()
                 ? schemeful_site.SerializeFileSiteWithHost()
                 : schemeful_site.Serialize());
    CHECK_EQ(serialized_partition_key->has_cross_site_ancestor(),
             has_cross_site_ancestor);
  } else {
    // If we cannot deserialize from string at all then top_level_site must be
    // opaque.
    CHECK(partition_key_from_url.site().opaque());
  }

  return 0;
}

}  // namespace net
