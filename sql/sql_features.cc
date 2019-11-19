// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

namespace sql {

namespace features {

// Skip the logic for preloading databases.
//
// Enabling this feature turns sql::Database::Preload() into a noop.
// https://crbug.com/243949 suggests that sql::Database::Preload() was added
// without any proper benchmarking, and the logic is a pessimization for modern
// OS schedulers.
//
// TODO(pwnall): After the performance impact of the change is assessed, remove
//               sql::Database::Preload() and this flag.
const base::Feature kSqlSkipPreload{"SqlSkipPreload",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace sql
