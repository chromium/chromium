// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_special_storage_policy.h"

#include "base/containers/contains.h"

namespace storage {

MockSpecialStoragePolicy::MockSpecialStoragePolicy() : all_unlimited_(false) {}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return base::Contains(protected_, origin);
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_)
    return true;
  return base::Contains(unlimited_, origin);
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return base::Contains(session_only_, origin);
}

bool MockSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return base::Contains(isolated_, origin);
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

bool MockSpecialStoragePolicy::IsStorageDurable(const GURL& origin) {
  return base::Contains(durable_, origin);
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() = default;

}  // namespace storage
