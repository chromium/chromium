// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATABASE_SQLITE_STATUS_HELPER_H_
#define STORAGE_COMMON_DATABASE_SQLITE_STATUS_HELPER_H_

#include "base/component_export.h"
#include "storage/common/database/db_status.h"

namespace sql {
class Database;
}

namespace storage {

// Creates a `DbStatus` using the last `database` operation's error code and
// message.
COMPONENT_EXPORT(STORAGE_DATABASE_STATUS)
DbStatus FromSqliteCode(const sql::Database& database);

}  // namespace storage

#endif  // STORAGE_COMMON_DATABASE_SQLITE_STATUS_HELPER_H_
