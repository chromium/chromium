// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_query_plan_test_util.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

base::FilePath GetExecPath(base::StringPiece name) {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AppendASCII(name);
}

}  // namespace

SqlIndexMatcher::SqlIndexMatcher() = default;
SqlIndexMatcher::SqlIndexMatcher(std::string name) : name_(std::move(name)) {}
SqlIndexMatcher::~SqlIndexMatcher() = default;

SqlIndexMatcher::SqlIndexMatcher(const SqlIndexMatcher&) = default;
SqlIndexMatcher& SqlIndexMatcher::operator=(const SqlIndexMatcher&) = default;

SqlQueryPlan::SqlQueryPlan(std::string query, std::string plan)
    : query_(std::move(query)), plan_(std::move(plan)) {}

SqlQueryPlan::~SqlQueryPlan() = default;

bool SqlQueryPlan::UsesIndex(const SqlIndexMatcher& matcher) const {
  base::StringPiece plan_piece(plan_);

  size_t start_pos = matcher.FindIndexStart(plan_piece);
  if (start_pos == std::string::npos) {
    return false;
  }

  size_t end_pos = plan_piece.find("\n", start_pos);

  base::StringPiece index_text =
      plan_piece.substr(start_pos, end_pos - start_pos);

  return base::ranges::all_of(matcher.columns(),
                              [index_text](const std::string& col) {
                                return base::Contains(index_text, col);
                              });
}

bool SqlQueryPlan::HasFullTableScan() const {
  return base::Contains(plan_, "SCAN");
}

size_t SqlIndexMatcher::FindIndexStart(base::StringPiece plan) const {
  std::string covering_prefix = base::StrCat({"USING COVERING INDEX ", name()});
  std::string noncovering_prefix = base::StrCat({"USING INDEX ", name()});
  std::string primary_prefix = "USING PRIMARY KEY ";
  switch (type()) {
    case SqlIndexMatcher::Type::kCovering:
      return plan.find(covering_prefix);
    case SqlIndexMatcher::Type::kPrimaryKey:
      DCHECK(name().empty());
      return plan.find(primary_prefix);
    case SqlIndexMatcher::Type::kAny:
      for (const base::StringPiece prefix :
           {covering_prefix, noncovering_prefix, primary_prefix}) {
        size_t pos = plan.find(prefix);
        if (pos != std::string::npos) {
          return pos;
        }
      }
      return std::string::npos;
  }
}

std::ostream& operator<<(std::ostream& out, const SqlQueryPlan& plan) {
  return out << plan.query_ << "\n" << plan.plan_;
}

SqlQueryPlanExplainer::SqlQueryPlanExplainer(base::FilePath db_path)
    : db_path_(std::move(db_path)),
      shell_path_(GetExecPath("sqlite_dev_shell")) {}

SqlQueryPlanExplainer::~SqlQueryPlanExplainer() = default;

absl::optional<SqlQueryPlan> SqlQueryPlanExplainer::GetPlan(
    std::string query,
    absl::optional<SqlFullScanReason> full_scan_reason) {
  base::CommandLine command_line(shell_path_);
  command_line.AppendArgPath(db_path_);

  std::string explain_query = base::StrCat({"EXPLAIN QUERY PLAN ", query});
  command_line.AppendArg(explain_query);

  std::string output;
  if (!base::GetAppOutputAndError(command_line, &output)) {
    return absl::nullopt;
  }

  if (!base::StartsWith(output, "QUERY PLAN")) {
    return absl::nullopt;
  }

  SqlQueryPlan plan(std::move(query), std::move(output));

  if (full_scan_reason.has_value()) {
    EXPECT_TRUE(plan.HasFullTableScan())
        << "Plan has out of date SqlFullScanReason. No full scan was found:\n"
        << plan;
  } else {
    EXPECT_FALSE(plan.HasFullTableScan())
        << "Plan has a full table scan, which must be "
           "annotated with a SqlFullScanReason:\n"
        << plan;
  }
  return plan;
}

}  // namespace content
