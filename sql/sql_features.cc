// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sql_features.h"

namespace sql::features {

// Clears the Database.db_ member if sqlite3_open_v2() fails.
// TODO(https://crbug.com/1441955): Remove this flag eventually.
BASE_FEATURE(kClearDbIfCloseFails,
             "ClearDbIfCloseFails",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable WAL mode for all SQLite databases.
BASE_FEATURE(kEnableWALModeByDefault,
             "EnableWALModeByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, `sql::BuiltInRecovery` can be used if it's supported. See
// https://crbug.com/1385500.
//
// This is an overarching kill switch which overrides any database-specific
// flag. See `sql::BuiltInRecovery::RecoverIfPossible()` for more context.
BASE_FEATURE(kUseBuiltInRecoveryIfSupported,
             "UseBuiltInRecoveryIfSupported",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace sql::features
