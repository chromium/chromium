// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "net/cookies/cookie_partition_key.h"

namespace net {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  std::string url_str = data_provider.ConsumeRandomLengthString(800);
  GURL url(url_str);
  if (!url.is_valid())
    return 0;

  SchemefulSite site(url::Origin::Create(url));
  absl::optional<CookiePartitionKey> partition_key =
      absl::make_optional(CookiePartitionKey(site));

  bool result = CookiePartitionKey::Deserialize(url_str, partition_key);
  if (site.opaque())
    CHECK(!result);
  else
    CHECK(result);

  std::string tmp;
  result = CookiePartitionKey::Serialize(partition_key, tmp);
  if (site.opaque())
    CHECK(!result);
  else
    CHECK(result);

  return 0;
}

}  // namespace net
