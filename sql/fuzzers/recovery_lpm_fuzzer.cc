// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This fuzzer constructs a DB from fuzzer-derived SQL statements and then
// mutates the file with fuzzer-derived XOR masks before exercising recovery.

#include <fuzzer/FuzzedDataProvider.h>

#include <cstdint>
#include <cstdlib>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_logging_settings.h"
#include "build/buildflag.h"
#include "sql/database.h"
#include "sql/fuzzers/sql_disk_corruption.pb.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/sqlite/fuzz/sql_query_grammar.pb.h"
#include "third_party/sqlite/fuzz/sql_query_proto_to_string.h"

namespace {

// usage: LPM_ADDITIONAL_ARGS="..." sql_recovery_lpm_fuzzer testcases...
//
// Positional args:
//   testcases                  One or more testcase files to run.
//
// Optional additional args (passed in through the LPM_ADDITIONAL_ARGS
// environment variable):
//   --dump_input               Prints the testcase file to the console in a
//   human readable format.
//   --out_db_path <file path>  Copies the database after it's been mutated to
//   the given path.

std::optional<base::CommandLine> GetCommandLine() {
  char* additional_args = std::getenv("LPM_ADDITIONAL_ARGS");
  if (additional_args == nullptr) {
    return std::nullopt;
  }
  std::vector<std::string> argv = base::SplitString(
      additional_args, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
#if BUILDFLAG(IS_WIN)
  std::vector<std::wstring> wargv(argv.size());
  base::ranges::transform(
      argv.begin(), argv.end(), wargv.begin(),
      [](std::string str) { return std::wstring(str.begin(), str.end()); });
  return base::CommandLine::FromArgvWithoutProgram(wargv);
#else
  return base::CommandLine::FromArgvWithoutProgram(argv);
#endif
}

// Initializes and manages state shared between fuzzer iterations. Use this to
// interact with global variables, environment variables, the filesystem, etc.
class Environment {
 public:
  Environment()
      : temp_dir_(MakeTempDir()),
        db_path_(GetTempFilePath("db.sqlite")),
        should_dump_input_(std::getenv("LPM_DUMP_NATIVE_INPUT") != nullptr) {
    auto command_line = GetCommandLine();
    if (command_line) {
      should_dump_input_ =
          should_dump_input_ || command_line->HasSwitch("dump_input");
      if (command_line->HasSwitch("out_db_path")) {
        out_db_path_ = MakeAbsoluteFilePath(
                           command_line->GetSwitchValuePath("out_db_path"))
                           .AppendASCII("db")
                           .AddExtensionASCII("sqlite");
      }
    }

    // Logging must be initialized before `ScopedLoggingSettings`. See
    // <https://crbug.com/331909454>.
    logging::InitLogging(logging::LoggingSettings{
        // The default logging destination on Windows is `LOG_TO_FILE`, which
        // would require us to set `LoggingSettings::log_file_path`.
        .logging_dest =
            logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR,
    });
    logging::SetMinLogLevel(logging::LOGGING_ERROR);
  }

  ~Environment() { AssertTempDirIsEmpty(); }

  // By convention, the LPM_DUMP_NATIVE_INPUT environment variable indicates
  // that the fuzzer should print its input in a readable format.
  bool should_dump_input() const { return should_dump_input_; }

  // The path to the database's backing file.
  const base::FilePath& db_path() const { return db_path_; }

  // The path the database is copied to after it's been mutated.
  const base::FilePath& out_db_path() const { return out_db_path_; }

  // Deletes the backing file and related journal files.
  void DeleteDbFiles() const {
    CHECK(base::DeleteFile(GetTempFilePath("db.sqlite")));
    CHECK(base::DeleteFile(GetTempFilePath("db.sqlite-journal")));
    CHECK(base::DeleteFile(GetTempFilePath("db.sqlite-wal")));
  }

  void AssertTempDirIsEmpty() const {
    if (base::IsDirectoryEmpty(temp_dir_.GetPath())) {
      return;
    }

    base::FileEnumerator files(temp_dir_.GetPath(), /*recursive=*/true,
                               base::FileEnumerator::FileType::FILES |
                                   base::FileEnumerator::FileType::DIRECTORIES);
    LOG(ERROR) << "Unexpected files or directories in temp dir:";
    files.ForEach(
        [](const base::FilePath& path) { LOG(ERROR) << "  " << path; });
    LOG(FATAL) << "Expected temp dir to be empty: " << temp_dir_.GetPath();
  }

 private:
  static base::ScopedTempDir MakeTempDir() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    base::CommandLine::Init(0, nullptr);
    base::FilePath shmem_temp_dir;
    CHECK(base::GetShmemTempDir(false, &shmem_temp_dir));
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDirUnderPath(shmem_temp_dir));
    return temp_dir;
#else
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    return temp_dir;
#endif
  }

  base::FilePath GetTempFilePath(std::string_view name) const {
    return temp_dir_.GetPath().AppendASCII(name);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  bool should_dump_input_ = false;
  base::FilePath out_db_path_;
};

// A wrapper around the fuzzer's input proto. Does some preprocessing to map the
// input to a higher-level test case.
class TestCase {
 public:
  // A single mutation instruction.
  struct Mutation {
    int64_t pos;
    uint64_t xor_mask;
  };

  explicit TestCase(const sql_fuzzers::RecoveryFuzzerTestCase& input)
      : strategy_(RecoveryStrategyFromInt(input.recovery_strategy())),
        wal_mode_(input.wal_mode()),
        sql_statement_(sql_fuzzer::SQLQueriesToString(input.queries())),
        sql_statement_after_open_(
            sql_fuzzer::SQLQueriesToString(input.queries_after_open())) {
    // Parse the input's `mutations` map as `Mutation` structs.
    mutations_.reserve(input.mutations_size());
    for (const auto& [pos, xor_mask] : input.mutations()) {
      // Ignore the zero mask because it is XOR's identity value.
      mutations_.emplace_back(pos, xor_mask ? xor_mask : 1);
    }
  }

  sql::Recovery::Strategy strategy() const { return strategy_; }
  bool wal_mode() const { return wal_mode_; }
  base::span<const Mutation> mutations() const { return mutations_; }
  base::cstring_view sql_statement() const { return sql_statement_; }
  base::cstring_view sql_statement_after_open() const {
    return sql_statement_after_open_;
  }

  // Print as a human-readable string.
  std::ostream& Print(std::ostream& os) const {
    os << "Test Case:" << std::endl;
    os << "- strategy: " << DebugFormat(strategy_) << std::endl;
    os << "- wal_mode: " << (wal_mode_ ? "true" : "false") << std::endl;
    os << "- mutations: " << std::endl;
    os << std::hex;
    for (const Mutation& mutation : mutations()) {
      os << "    {pos=0x" << mutation.pos << ", xor_mask=0x"
         << mutation.xor_mask << "}," << std::endl;
    }
    os << std::dec;
    os << "- sql_statement: " << DebugFormat(sql_statement()) << std::endl;
    os << "- sql_statement_after_open: "
       << DebugFormat(sql_statement_after_open()) << std::endl;
    return os;
  }

 private:
  // Converts an arbitrary int to a valid enum value.
  static sql::Recovery::Strategy RecoveryStrategyFromInt(int input);
  // Converts arbitrary bytes in `s` to a human-readable ASCII string.
  // Non-printable characters are hex-escaped.
  static std::string DebugFormat(std::string_view s);
  // Converts the value of `strategy`, which must be a valid enum value, to a
  // human-readable string.
  static constexpr const char* DebugFormat(sql::Recovery::Strategy strategy);

  // Fields parsed from the fuzzer input:
  const sql::Recovery::Strategy strategy_ =
      sql::Recovery::Strategy::kRecoverOrRaze;
  const bool wal_mode_ = false;
  std::vector<Mutation> mutations_;
  const std::string sql_statement_;
  const std::string sql_statement_after_open_;
};

std::ostream& operator<<(std::ostream& os, const TestCase& test_case) {
  return test_case.Print(os);
}

}  // namespace

DEFINE_PROTO_FUZZER(const sql_fuzzers::RecoveryFuzzerTestCase& fuzzer_input) {
  static Environment env;

  // Ignore this input if it includes any "ATTACH DATABASE" queries. These
  // queries may cause SQLite to create files like `file::memory:` in the
  // current working directory, which is undesirable. (See how `AttachDatabase`
  // is handled in //third_party/sqlite/fuzz/sql_query_proto_to_string.cc.)
  //
  // TODO: A slight improvement would be to filter out individual "ATTACH
  // DATABASE" queries rather than throwing away the whole test case.
  if (base::ranges::any_of(fuzzer_input.queries().extra_queries(),
                           &sql_query_grammar::SQLQuery::has_attach_db) ||
      base::ranges::any_of(fuzzer_input.queries_after_open().extra_queries(),
                           &sql_query_grammar::SQLQuery::has_attach_db)) {
    return;
  }

  // The purpose of this fuzzer is to throw *corrupted* database files at the
  // recovery module. If there are no mutations, this test case is out of scope.
  if (fuzzer_input.mutations().empty()) {
    return;
  }

  TestCase test_case(fuzzer_input);

  if (env.should_dump_input()) {
    std::cout << test_case;
  }

  sql::DatabaseOptions database_options;
  database_options.wal_mode = test_case.wal_mode();
  sql::Database database(database_options);
  CHECK(database.Open(env.db_path()));

  // Bootstrap the database with SQL queries derived from `fuzzer_input`.
  {
    // SQLite may warn us about errors in these queries, e.g. "unknown database
    // foo". Temporarily silence those warnings.
    logging::ScopedLoggingSettings scoped_logging;
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    std::ignore = database.Execute(test_case.sql_statement());
  }
  database.Close();

  // Mutate the backing file. Skip the expensive file operations when there are
  // no bytes to mutate.
  int64_t file_length;
  CHECK(base::GetFileSize(env.db_path(), &file_length));
  if (file_length > 0) {
    base::File file(env.db_path(), base::File::FLAG_OPEN |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE);
    // Apply each mutation without sorting by file position. These random-access
    // file operations could be a performance bottleneck if the temp directory
    // is on a physical disk.
    for (TestCase::Mutation mutation : test_case.mutations()) {
      // File read/write operations expect positions to point within the file.
      mutation.pos %= file_length;
      if (mutation.pos < 0) {
        mutation.pos = 0;
      }

      uint64_t buf = 0;
      const int num_read =
          file.Read(mutation.pos, reinterpret_cast<char*>(&buf), sizeof(buf));
      CHECK_NE(num_read, -1);
      if (num_read == 0) {
        continue;
      }

      buf ^= mutation.xor_mask;

      // Write `buf` back to the file, being careful not to add bytes to the
      // file that did not exist before.
      CHECK_NE(
          file.Write(mutation.pos, reinterpret_cast<char*>(&buf), num_read),
          -1);
    }
    CHECK_EQ(file_length, file.GetLength());
  }

  if (!env.out_db_path().empty()) {
    base::CopyFile(env.db_path(), env.out_db_path());
  }

  bool attempted_recovery = false;
  auto error_callback =
      base::BindLambdaForTesting([&](int extended_error, sql::Statement*) {
        if (!attempted_recovery) {
          attempted_recovery = sql::Recovery::RecoverIfPossible(
              &database, extended_error, test_case.strategy());
        }
      });
  database.set_error_callback(std::move(error_callback));

  // Reopen the database after potentially corrupting the file. This may run
  // the error callback.
  const bool opened = database.Open(env.db_path());
  if (opened) {
    logging::ScopedLoggingSettings scoped_logging;
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    std::ignore = database.Execute(test_case.sql_statement_after_open());

    database.Close();
  }

  // Delete the backing file to prepare for the next iteration.
  env.DeleteDbFiles();
  // Ensure that no unexpected files were created in the temp directory.
  env.AssertTempDirIsEmpty();
}

namespace {

sql::Recovery::Strategy TestCase::RecoveryStrategyFromInt(int input) {
  static_assert(
      std::is_same_v<std::underlying_type<sql::Recovery::Strategy>::type,
                     decltype(input)>,
      "sql::Recovery::Strategy's underlying type must match the input");

  const auto strategy = static_cast<sql::Recovery::Strategy>(input);

  // Ensure that we remember to update the fuzzer if more strategies are added.
  switch (strategy) {
    case sql::Recovery::Strategy::kRecoverOrRaze:
    case sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze:
      return strategy;
  }
  // When `input` is out of range, return a default value.
  return sql::Recovery::Strategy::kRecoverOrRaze;
}

std::string TestCase::DebugFormat(std::string_view s) {
  std::string out;
  out.reserve(s.length() + 2);
  out.push_back('"');
  for (char c : s) {
    if (base::IsAsciiPrintable(c)) {
      out.push_back(c);
    } else {
      out.push_back('\\');
      out.push_back('x');
      base::AppendHexEncodedByte(static_cast<uint8_t>(c), /*output=*/out);
    }
  }
  out.push_back('"');
  return out;
}

constexpr const char* TestCase::DebugFormat(sql::Recovery::Strategy strategy) {
  switch (strategy) {
    case sql::Recovery::Strategy::kRecoverOrRaze:
      return "kRecoverOrRaze";
    case sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze:
      return "kRecoverWithMetaVersionOrRaze";
  }
}

}  // namespace
