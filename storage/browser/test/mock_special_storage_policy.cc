// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_special_storage_policy.h"


namespace storage {

MockSpecialStoragePolicy::MockSpecialStoragePolicy() : all_unlimited_(false) {}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return protected_.contains(origin);
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_) {
    return true;
  }
  return unlimited_.contains(origin);
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return session_only_.contains(origin);
}

bool MockSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return isolated_.contains(origin);
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

bool MockSpecialStoragePolicy::IsStoragePersistent(const GURL& origin) {
  return persistent_.contains(origin);
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() = default;

}  // namespace storage
