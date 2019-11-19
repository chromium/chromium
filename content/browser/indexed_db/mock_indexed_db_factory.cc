// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_indexed_db_factory.h"

#include <vector>

namespace content {

MockIndexedDBFactory::MockIndexedDBFactory() {
}

MockIndexedDBFactory::~MockIndexedDBFactory() {
}

std::vector<IndexedDBDatabase*> MockIndexedDBFactory::GetOpenDatabasesForOrigin(
    const url::Origin& origin) const {
  return std::vector<IndexedDBDatabase*>();
}

}  // namespace content
