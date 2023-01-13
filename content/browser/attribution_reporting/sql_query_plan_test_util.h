// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_piece_forward.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// TODO(crbug.com/1407007): Move these utilities to //sql once they are trialed
// in the attribution_reporting directory.
//
// Class which specifies how to match an index in a query plan.
class SqlIndexMatcher {
 public:
  // Only matches an index with the given name. Note, for primary key indices,
  // this is unused, so the constructor without the name field should be used.
  explicit SqlIndexMatcher(std::string name);
  SqlIndexMatcher();
  ~SqlIndexMatcher();

  SqlIndexMatcher(const SqlIndexMatcher&);
  SqlIndexMatcher& operator=(const SqlIndexMatcher& o);

  // Every sqlite index includes a list of indexed columns. However, some query
  // plans will only use a subset of the columns in the index. This matcher is
  // designed to enforce that a given subset of columns are actually used by the
  // query planner. Note that this list doesn't have to be exhaustive, and
  // plans that use a superset of columns than ones listed in `columns` will
  // still match.
  SqlIndexMatcher& set_columns(std::vector<std::string> columns) {
    columns_ = std::move(columns);
    return *this;
  }

  // Specifies the type of index that we should match with. Note this also
  // covers primary keys which are implemented as indexes in sqlite.
  enum class Type {
    kAny,         // USING INDEX, or any of the other options with match
    kCovering,    // USING COVERING INDEX
    kPrimaryKey,  // USING PRIMARY KEY
  };
  SqlIndexMatcher& set_type(Type type) {
    type_ = type;
    return *this;
  }

  const std::string& name() const { return name_; }
  const std::vector<std::string>& columns() const { return columns_; }
  Type type() const { return type_; }

  size_t FindIndexStart(base::StringPiece plan) const;

 private:
  std::string name_;
  std::vector<std::string> columns_;
  Type type_ = Type::kAny;
};

// This class wraps a returned sqlite query plan, as returned from EXPLAIN QUERY
// PLAN, and allows making various checks on its properties.
class SqlQueryPlan {
 public:
  SqlQueryPlan(std::string query, std::string plan);
  ~SqlQueryPlan();

  // Returns true if the query plan uses the index specified by `index`.
  [[nodiscard]] bool UsesIndex(const SqlIndexMatcher& matcher) const;

  // Returns true if the query plan does any full table scan, i.e. it includes a
  // SCAN directive.
  [[nodiscard]] bool HasFullTableScan() const;

 private:
  friend std::ostream& operator<<(std::ostream& out, const SqlQueryPlan& plan);

  const std::string query_;
  const std::string plan_;
};

// Only for diagnostic messages, not for test expectations, as the underlying
// query plan format is fragile and should not be relied upon.
std::ostream& operator<<(std::ostream& out, const SqlQueryPlan& plan);

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
  explicit SqlQueryPlanExplainer(base::FilePath db_path);

  ~SqlQueryPlanExplainer();

  // Returns the query plan for a given query. Returns nullopt on error. Makes
  // test expectations based on the SqlFullScanReason, which enforces by default
  // that query plans should not perform full table scans and must be annotated
  // when they do.
  absl::optional<SqlQueryPlan> GetPlan(
      std::string query,
      absl::optional<SqlFullScanReason> full_scan_reason = absl::nullopt);

 private:
  const base::FilePath db_path_;
  const base::FilePath shell_path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_QUERY_PLAN_TEST_UTIL_H_
