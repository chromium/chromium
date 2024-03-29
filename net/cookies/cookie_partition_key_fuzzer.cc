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
  if (!url.is_valid())
    return 0;

  std::optional<CookiePartitionKey> partition_key =
      std::make_optional(CookiePartitionKey::FromURLForTesting(url));

  bool is_opaque = url::Origin::Create(url).opaque();
  CHECK_NE(is_opaque, CookiePartitionKey::Serialize(partition_key).has_value());

  CHECK_NE(is_opaque, CookiePartitionKey::FromUntrustedInput(
                          url_str, partition_key->IsThirdParty())
                          .has_value());

  if (!is_opaque) {
    CHECK(std::make_optional(CookiePartitionKey::FromURLForTesting(url)) ==
          partition_key);
  }

  return 0;
}

}  // namespace net
