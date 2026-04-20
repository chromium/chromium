// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/test_util.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace disk_cache::test {

namespace {

base::FilePath GetExecSqlShellPath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AppendASCII("sqlite_dev_shell");
}

}  // namespace

std::string GetQueryPlan(const base::FilePath& db_path,
                         std::string_view query) {
  base::CommandLine command_line(GetExecSqlShellPath());
  command_line.AppendArgPath(db_path);

  std::string explain_query = base::StrCat({"EXPLAIN QUERY PLAN ", query});
  command_line.AppendArg(explain_query);

  std::string output;
  if (!base::GetAppOutput(command_line, &output)) {
    return "Failed to execute sqlite_dev_shell";
  }
  base::TrimWhitespaceASCII(output, base::TRIM_ALL, &output);
  if (auto temp = base::RemovePrefix(output, "QUERY PLAN")) {
    output = base::TrimWhitespaceASCII(*temp, base::TRIM_LEADING);
  }
  return output;
}

}  // namespace disk_cache::test
