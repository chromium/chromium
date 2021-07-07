// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage.h"

#include <utility>

#include "content/browser/aggregation_service/public_key.h"

namespace content {

AggregationServiceStorage::AggregationServiceStorage() = default;

AggregationServiceStorage::~AggregationServiceStorage() = default;

PublicKeysForOrigin AggregationServiceStorage::GetPublicKeys(
    const url::Origin& origin) const {
  auto it = public_keys_map_.find(origin);
  if (it != public_keys_map_.end()) {
    return PublicKeysForOrigin(origin, it->second);
  } else {
    return PublicKeysForOrigin(origin, {});
  }
}

void AggregationServiceStorage::SetPublicKeys(const PublicKeysForOrigin& keys) {
  public_keys_map_.insert_or_assign(keys.origin, keys.keys);
}

void AggregationServiceStorage::ClearPublicKeys(const url::Origin& origin) {
  public_keys_map_.erase(origin);
}

}  // namespace content