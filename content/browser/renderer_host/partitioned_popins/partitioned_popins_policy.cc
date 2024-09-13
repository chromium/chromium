// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/partitioned_popins/partitioned_popins_policy.h"

#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace content {

PartitionedPopinsPolicy::PartitionedPopinsPolicy(std::string untrusted_input) {
  const std::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(untrusted_input);
  if (!dict) {
    // End early if malformed.
    return;
  }
  const auto& idx = dict->find("partitioned");
  if (idx == dict->end()) {
    // End early if partitioned key is missing.
    return;
  }
  std::vector<url::Origin> potential_origins;
  for (const auto& parameterized_item : idx->second.member) {
    if (parameterized_item.item.is_token() &&
        parameterized_item.item.GetString() == "*") {
      // End early if this is a wildcard policy.
      wildcard = true;
      return;
    } else if (parameterized_item.item.is_string()) {
      url::Origin origin =
          url::Origin::Create(GURL(parameterized_item.item.GetString()));
      if (origin.scheme() == url::kHttpsScheme) {
        potential_origins.push_back(origin);
      }
    }
  }
  // Only update this with valid origins for non-wildcard policies.
  origins = potential_origins;
}

PartitionedPopinsPolicy::~PartitionedPopinsPolicy() = default;

}  // namespace content
