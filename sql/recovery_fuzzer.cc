// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"

namespace {

// Does initialization and holds state that's shared across all runs.
class Environment {
 public:
  Environment() {
    CHECK(base::CreateTemporaryFile(&data_file_path_));
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

  ~Environment() { base::DeleteFile(data_file_path_); }

  const base::FilePath& data_file_path() const { return data_file_path_; }

 private:
  base::FilePath data_file_path_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Prepare fuzzed data file.
  CHECK(base::WriteFile(env.data_file_path(), base::make_span(data, size)));

  // Load database. Check there's no unrecoverable error.
  sql::DatabaseOptions options;
  sql::Database database(options);
  bool should_attempt_recovery = false;
  database.set_error_callback(
      base::BindLambdaForTesting([&](int extended_error, sql::Statement*) {
        should_attempt_recovery = sql::Recovery::ShouldRecover(extended_error);
      }));
  std::ignore = database.Open(env.data_file_path());

  // Attempt recovery.
  if (should_attempt_recovery) {
    database.reset_error_callback();
    sql::Recovery::RecoverDatabase(&database, env.data_file_path());
  }

  return 0;
}
