// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_TEST_UTIL_H_
#define NET_DISK_CACHE_SQL_TEST_UTIL_H_

#include <string>
#include <string_view>

#include "base/files/file_path.h"

namespace disk_cache::test {

// Executes `EXPLAIN QUERY PLAN` on the given `query` against the database at
// `db_path` using the `sqlite_dev_shell` tool. Returns the query plan as a
// string, or an error message if the execution fails.
std::string GetQueryPlan(const base::FilePath& db_path, std::string_view query);

}  // namespace disk_cache::test

#endif  // NET_DISK_CACHE_SQL_TEST_UTIL_H_
