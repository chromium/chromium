// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/features.h"

namespace content::features {

// Guard for work on https://crbug.com/40279485
BASE_FEATURE(kIndexedDBShardBackingStores,
             "IndexedDBShardBackingStores",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content::features
