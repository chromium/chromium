// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

#include "base/feature_list.h"

namespace sql::features {

// Explicitly unlock the database on close to ensure lock is released.
BASE_FEATURE(kUnlockDatabaseOnClose,
             "UnlockDatabaseOnClose",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace sql::features
