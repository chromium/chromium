// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_factory.h"

#include <vector>

namespace content {

MockIndexedDBFactory::MockIndexedDBFactory() {
}

MockIndexedDBFactory::~MockIndexedDBFactory() {
}

std::vector<IndexedDBDatabase*> MockIndexedDBFactory::GetOpenDatabasesForBucket(
    const storage::BucketLocator& bucket_locator) const {
  return std::vector<IndexedDBDatabase*>();
}

}  // namespace content
