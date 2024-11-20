// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sql/statement.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// static
int64_t Statement::TimeToSqlValue(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// This empty constructor initializes our reference with an empty one so that
// we don't have to null-check the ref_ to see if the statement is valid: we
// only have to check the ref's validity bit.
Statement::Statement()
    : ref_(base::MakeRefCounted<Database::StatementRef>(nullptr,
                                                        nullptr,
                                                        false)) {}

Statement::Statement(scoped_refptr<Database::StatementRef> ref)
    : ref_(std::move(ref)) {}

Statement::~Statement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Free the resources associated with this statement. We assume there's only
  // one statement active for a given sqlite3_stmt at any time, so this won't
  // mess with anything.
  Reset(true);
}

void Statement::Assign(scoped_refptr<Database::StatementRef> ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Reset(true);
  ref_ = std::move(ref);
}

void Statement::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Assign(base::MakeRefCounted<Database::StatementRef>(nullptr, nullptr, false));
  succeeded_ = false;
}

bool Statement::CheckValid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Allow operations to fail silently if a statement was invalidated
  // because the database was closed by an error handler.
  DLOG_IF(FATAL, !ref_->was_valid())
      << "Cannot call mutating statements on an invalid statement.";
  return is_valid();
}

SqliteResultCode Statement::StepInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CheckValid())
    return SqliteResultCode::kError;

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  auto sqlite_result_code = ToSqliteResultCode(sqlite3_step(ref_->stmt()));
  return CheckSqliteResultCode(sqlite_result_code);
}

void Statement::ReportQueryExecutionMetrics() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Retrieve and reset to zero the count of VM steps required to execute the
  // query. The reported UMA metric can be used to identify expensive database
  // based on their SQLite queries cost in VM steps.
  const int kResetVMStepsToZero = 1;
  const int vm_steps = sqlite3_stmt_status(
      ref_->stmt(), SQLITE_STMTSTATUS_VM_STEP, kResetVMStepsToZero);
  if (vm_steps > 0) {
    const Database* database = ref_->database();
    if (!database->histogram_tag().empty()) {
      const std::string histogram_name =
          "Sql.Statement." + database->histogram_tag() + ".VMSteps";
      base::UmaHistogramCounts10000(histogram_name, vm_steps);
    }
  }
}

bool Statement::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << "Run() must be called exactly once";
  run_called_ = true;
  DCHECK(!step_called_) << "Run() must not be mixed with Step()";
#endif  // DCHECK_IS_ON()
  return StepInternal() == SqliteResultCode::kDone;
}

bool Statement::Step() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << "Run() must not be mixed with Step()";
  step_called_ = true;
#endif  // DCHECK_IS_ON()
  return StepInternal() == SqliteResultCode::kRow;
}

void Statement::Reset(bool clear_bound_vars) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);
  if (is_valid()) {
    // Reports the execution cost for this SQL statement.
    ReportQueryExecutionMetrics();

    if (clear_bound_vars)
      sqlite3_clear_bindings(ref_->stmt());

    // StepInternal() cannot track success because statements may be reset
    // before reaching SQLITE_DONE.  Don't call CheckError() because
    // sqlite3_reset() returns the last step error, which StepInternal() already
    // checked.
    sqlite3_reset(ref_->stmt());
  }

  // Potentially release dirty cache pages if an autocommit statement made
  // changes.
  if (ref_->database())
    ref_->database()->ReleaseCacheMemoryIfNeeded(false);

  succeeded_ = false;
#if DCHECK_IS_ON()
  run_called_ = false;
  step_called_ = false;
#endif  // DCHECK_IS_ON()
}

bool Statement::Succeeded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return is_valid() && succeeded_;
}

void Statement::BindNull(int param_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int sqlite_result_code = sqlite3_bind_null(ref_->stmt(), param_index + 1);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindBool(int param_index, bool val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BindInt64(param_index, val ? 1 : 0);
}

void Statement::BindInt(int param_index, int val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int sqlite_result_code = sqlite3_bind_int(ref_->stmt(), param_index + 1, val);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindInt64(int param_index, int64_t val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int sqlite_result_code =
      sqlite3_bind_int64(ref_->stmt(), param_index + 1, val);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindDouble(int param_index, double val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int sqlite_result_code =
      sqlite3_bind_double(ref_->stmt(), param_index + 1, val);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindTime(int param_index, base::Time val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int64_t int_value = TimeToSqlValue(val);
  int sqlite_result_code =
      sqlite3_bind_int64(ref_->stmt(), param_index + 1, int_value);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindTimeDelta(int param_index, base::TimeDelta delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid()) {
    return;
  }

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int64_t int_value = delta.InMicroseconds();
  int sqlite_result_code =
      sqlite3_bind_int64(ref_->stmt(), param_index + 1, int_value);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindCString(int param_index, const char* val) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  DCHECK(val);
  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";

  // If the string length is more than SQLITE_MAX_LENGTH (or the per-database
  // SQLITE_LIMIT_LENGTH limit), sqlite3_bind_text() fails with SQLITE_TOOBIG.
  //
  // We're not currently handling this error. SQLITE_MAX_LENGTH is set to the
  // default (1 billion bytes) in Chrome's SQLite build, so this is an unlilely
  // issue.

  int sqlite_result_code = sqlite3_bind_text(ref_->stmt(), param_index + 1, val,
                                             -1, SQLITE_TRANSIENT);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindString(int param_index, std::string_view value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";

  // std::string_view::data() may return null for empty pieces. In particular,
  // this may happen when the std::string_view is created from the default
  // constructor.
  //
  // However, sqlite3_bind_text() always interprets a nullptr data argument as a
  // NULL value, instead of an empty BLOB value.
  static constexpr char kEmptyPlaceholder[] = {0x00};
  const char* data = (value.size() > 0) ? value.data() : kEmptyPlaceholder;

  // If the string length is more than SQLITE_MAX_LENGTH (or the per-database
  // SQLITE_LIMIT_LENGTH limit), sqlite3_bind_text() fails with SQLITE_TOOBIG.
  //
  // We're not currently handling this error. SQLITE_MAX_LENGTH is set to the
  // default (1 billion bytes) in Chrome's SQLite build, so this is an unlilely
  // issue.

  int sqlite_result_code = sqlite3_bind_text(
      ref_->stmt(), param_index + 1, data, value.size(), SQLITE_TRANSIENT);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

void Statement::BindString16(int param_index, std::u16string_view value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return BindString(param_index, base::UTF16ToUTF8(value));
}

void Statement::BindBlob(int param_index, base::span<const uint8_t> value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " must not be called after Run()";
  DCHECK(!step_called_) << __func__ << " must not be called after Step()";
#endif  // DCHECK_IS_ON()

  if (!is_valid())
    return;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";

  // span::data() may return null for empty spans. In particular, this may
  // happen when the span is created out of a std::vector, because
  // std::vector::data() may (or may not) return null for empty vectors.
  //
  // However, sqlite3_bind_blob() always interprets a nullptr data argument as a
  // NULL value, instead of an empty BLOB value.
  //
  // While the difference between NULL and an empty BLOB may not matter in some
  // cases, it may also cause subtle bugs in other cases. So, we cannot pass
  // span.data() directly to sqlite3_bind_blob().
  static constexpr uint8_t kEmptyPlaceholder[] = {0x00};
  const uint8_t* data = (value.size() > 0) ? value.data() : kEmptyPlaceholder;

  // If the string length is more than SQLITE_MAX_LENGTH (or the per-database
  // SQLITE_LIMIT_LENGTH limit), sqlite3_bind_text() fails with SQLITE_TOOBIG.
  //
  // We're not currently handling this error. SQLITE_MAX_LENGTH is set to the
  // default (1 billion bytes) in Chrome's SQLite build, so this is an unlilely
  // issue.

  int sqlite_result_code = sqlite3_bind_blob(
      ref_->stmt(), param_index + 1, data, value.size(), SQLITE_TRANSIENT);
  DCHECK_EQ(sqlite_result_code, SQLITE_OK);
}

int Statement::ColumnCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_valid())
    return 0;
  return sqlite3_column_count(ref_->stmt());
}

// Verify that our enum matches sqlite's values.
static_assert(static_cast<int>(ColumnType::kInteger) == SQLITE_INTEGER,
              "INTEGER mismatch");
static_assert(static_cast<int>(ColumnType::kFloat) == SQLITE_FLOAT,
              "FLOAT mismatch");
static_assert(static_cast<int>(ColumnType::kText) == SQLITE_TEXT,
              "TEXT mismatch");
static_assert(static_cast<int>(ColumnType::kBlob) == SQLITE_BLOB,
              "BLOB mismatch");
static_assert(static_cast<int>(ColumnType::kNull) == SQLITE_NULL,
              "NULL mismatch");

ColumnType Statement::GetColumnType(int col) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  return static_cast<enum ColumnType>(sqlite3_column_type(ref_->stmt(), col));
}

bool Statement::ColumnBool(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return static_cast<bool>(ColumnInt64(column_index));
}

int Statement::ColumnInt(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return 0;
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  return sqlite3_column_int(ref_->stmt(), column_index);
}

int64_t Statement::ColumnInt64(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return 0;
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  return sqlite3_column_int64(ref_->stmt(), column_index);
}

double Statement::ColumnDouble(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return 0;
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  return sqlite3_column_double(ref_->stmt(), column_index);
}

base::Time Statement::ColumnTime(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return base::Time();
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  int64_t int_value = sqlite3_column_int64(ref_->stmt(), column_index);
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(int_value));
}

base::TimeDelta Statement::ColumnTimeDelta(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid()) {
    return base::TimeDelta();
  }
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  int64_t int_value = sqlite3_column_int64(ref_->stmt(), column_index);
  return base::Microseconds(int_value);
}

std::string_view Statement::ColumnStringView(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return std::string_view();
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  const char* string_buffer = reinterpret_cast<const char*>(
      sqlite3_column_text(ref_->stmt(), column_index));
  int size = sqlite3_column_bytes(ref_->stmt(), column_index);
  DCHECK(size == 0 || string_buffer != nullptr)
      << "sqlite3_column_text() returned a null buffer for a non-empty string";

  return std::string_view(string_buffer, base::checked_cast<size_t>(size));
}

std::string Statement::ColumnString(int column_index) {
  return std::string(ColumnStringView(column_index));
}

std::u16string Statement::ColumnString16(int column_index) {
  return base::UTF8ToUTF16(ColumnStringView(column_index));
}

base::span<const uint8_t> Statement::ColumnBlob(int column_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return base::span<const uint8_t>();
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  int result_size = sqlite3_column_bytes(ref_->stmt(), column_index);
  const void* result_buffer = sqlite3_column_blob(ref_->stmt(), column_index);
  DCHECK(result_size == 0 || result_buffer != nullptr)
      << "sqlite3_column_blob() returned a null buffer for a non-empty BLOB";

  return base::make_span(static_cast<const uint8_t*>(result_buffer),
                         base::checked_cast<size_t>(result_size));
}

bool Statement::ColumnBlobAsString(int column_index, std::string* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return false;
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  const void* result_buffer = sqlite3_column_blob(ref_->stmt(), column_index);
  int size = sqlite3_column_bytes(ref_->stmt(), column_index);
  if (result_buffer && size > 0) {
    result->assign(reinterpret_cast<const char*>(result_buffer), size);
  } else {
    result->clear();
  }
  return true;
}

bool Statement::ColumnBlobAsString16(int column_index, std::u16string* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(result);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid()) {
    return false;
  }
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  const void* result_buffer = sqlite3_column_blob(ref_->stmt(), column_index);
  int size = sqlite3_column_bytes(ref_->stmt(), column_index);
  if (result_buffer && size > 0) {
    result->assign(reinterpret_cast<const char16_t*>(result_buffer), size / 2);
  } else {
    result->clear();
  }
  return true;
}

bool Statement::ColumnBlobAsVector(int column_index,
                                   std::vector<char>* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!run_called_) << __func__ << " can be used after Step(), not Run()";
  DCHECK(step_called_) << __func__ << " can only be used after Step()";
#endif  // DCHECK_IS_ON()

  if (!CheckValid())
    return false;
  DCHECK_GE(column_index, 0);
  DCHECK_LT(column_index, sqlite3_data_count(ref_->stmt()))
      << "Invalid column index";

  const void* result_buffer = sqlite3_column_blob(ref_->stmt(), column_index);
  int size = sqlite3_column_bytes(ref_->stmt(), column_index);
  if (result_buffer && size > 0) {
    // Unlike std::string, std::vector does not have an assign() overload that
    // takes a buffer and a size.
    result->assign(static_cast<const char*>(result_buffer),
                   static_cast<const char*>(result_buffer) + size);
  } else {
    result->clear();
  }
  return true;
}

bool Statement::ColumnBlobAsVector(int column_index,
                                   std::vector<uint8_t>* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return ColumnBlobAsVector(column_index,
                            reinterpret_cast<std::vector<char>*>(result));
}

std::string Statement::GetSQLStatement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // SQLite promises to keep the returned buffer alive until the statement is
  // finalized. We immediately copy the buffer contents into a std::string so we
  // don't need to worry about its lifetime. The performance overhead is
  // acceptable because this method should only be invoked for logging details
  // about SQLite errors.
  //
  // We use sqlite3_sql() instead of sqlite3_expanded_sql() because:
  //  - The returned SQL string matches the source code, making it easy to
  //    search.
  //  - This works with SQL statements that work with large data, such as BLOBS
  //    storing images.
  //  - The returned string is free of bound values, so it does not contain any
  //    PII that would raise privacy concerns around logging.
  //
  // Do not change this to use sqlite3_expanded_sql(). If that need ever arises
  // in the future, make a new function instead listing the above caveats.
  //
  // See https://www.sqlite.org/c3ref/expanded_sql.html for more details on the
  // difference between sqlite3_sql() and sqlite3_expanded_sql().
  return sqlite3_sql(ref_->stmt());
}

SqliteResultCode Statement::CheckSqliteResultCode(
    SqliteResultCode sqlite_result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  succeeded_ = IsSqliteSuccessCode(sqlite_result_code);
  if (!succeeded_ && ref_.get() && ref_->database()) {
    auto sqlite_error_code = ToSqliteErrorCode(sqlite_result_code);
    ref_->database()->OnSqliteError(sqlite_error_code, this, nullptr);
  }
  return sqlite_result_code;
}

}  // namespace sql
