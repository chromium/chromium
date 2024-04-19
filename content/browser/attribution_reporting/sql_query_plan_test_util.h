// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// TODO(crbug.com/40252981): Move these utilities to //sql once they are trialed
// in the attribution_reporting directory.

// This type wraps a returned sqlite query plan, as returned from EXPLAIN QUERY
// PLAN.
struct SqlQueryPlan {
  std::string query;
  std::string plan;
};

// Only for diagnostic messages, not for test expectations, as the underlying
// query plan format is fragile and should not be relied upon.
std::ostream& operator<<(std::ostream&, const SqlQueryPlan&);

testing::Matcher<SqlQueryPlan> UsesIndex(std::string name,
                                         std::vector<std::string> columns = {});

testing::Matcher<SqlQueryPlan> UsesCoveringIndex(
    std::string name,
    std::vector<std::string> columns = {});

testing::Matcher<SqlQueryPlan> UsesPrimaryKey();

// Enum explaining why a full scan on a query is needed. For use when annotating
// tests which use queries that perform a full table scan.
enum class SqlFullScanReason {
  // The full table scan is intentional, the query has to perform one.
  kIntentional,
  // The full table scan is not intentional and is only present because the
  // query has not been optimized.
  kNotOptimized,
};

// This class runs the sqlite_dev_shell to call EXPLAIN QUERY PLAN on provided
// queries, in the context of the passed in `db_path`.
class SqlQueryPlanExplainer {
 public:
  enum class Error {
    kCommandFailed,
    kInvalidOutput,
    kMissingFullScanAnnotation,
    kExtraneousFullScanAnnotation,
  };

  explicit SqlQueryPlanExplainer(base::FilePath db_path);

  ~SqlQueryPlanExplainer();

  // Returns the query plan for a given query. Returns nullopt on error. Makes
  // test expectations based on the SqlFullScanReason, which enforces by default
  // that query plans should not perform full table scans and must be annotated
  // when they do.
  base::expected<SqlQueryPlan, Error> GetPlan(
      std::string query,
      std::optional<SqlFullScanReason> full_scan_reason = std::nullopt);

 private:
  const base::FilePath db_path_;
  const base::FilePath shell_path_;
};

std::ostream& operator<<(std::ostream&, SqlQueryPlanExplainer::Error);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_
