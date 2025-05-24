// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

#include "base/feature_list.h"

namespace sql::features {

// Use a fixed memory-map size instead of using the heuristic.
BASE_FEATURE(kSqlFixedMmapSize,
             "SqlFixedMmapSize",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Explicitly unlock the database on close to ensure lock is released.
BASE_FEATURE(kUnlockDatabaseOnClose,
             "UnlockDatabaseOnClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sql::features
