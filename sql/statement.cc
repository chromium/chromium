// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"  // TODO(crbug.com/866218): Remove this include.
#include "third_party/sqlite/sqlite3.h"

namespace sql {

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
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // !defined(OS_ANDROID)

  // Free the resources associated with this statement. We assume there's only
  // one statement active for a given sqlite3_stmt at any time, so this won't
  // mess with anything.
  Reset(true);
}

void Statement::Assign(scoped_refptr<Database::StatementRef> ref) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // !defined(OS_ANDROID)

  Reset(true);
  ref_ = std::move(ref);
}

void Statement::Clear() {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // !defined(OS_ANDROID)

  Assign(base::MakeRefCounted<Database::StatementRef>(nullptr, nullptr, false));
  succeeded_ = false;
}

bool Statement::CheckValid() const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // !defined(OS_ANDROID)

  // Allow operations to fail silently if a statement was invalidated
  // because the database was closed by an error handler.
  DLOG_IF(FATAL, !ref_->was_valid())
      << "Cannot call mutating statements on an invalid statement.";
  return is_valid();
}

int Statement::StepInternal() {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return SQLITE_ERROR;

  absl::optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);

  stepped_ = true;
  int ret = sqlite3_step(ref_->stmt());
  return CheckError(ret);
}

bool Statement::Run() {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  DCHECK(!stepped_);
  return StepInternal() == SQLITE_DONE;
}

bool Statement::Step() {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return StepInternal() == SQLITE_ROW;
}

void Statement::Reset(bool clear_bound_vars) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  absl::optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(FROM_HERE, &scoped_blocking_call);
  if (is_valid()) {
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
  stepped_ = false;
}

bool Statement::Succeeded() const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return is_valid() && succeeded_;
}

bool Statement::BindNull(int param_index) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  return sqlite3_bind_null(ref_->stmt(), param_index + 1) == SQLITE_OK;
}

bool Statement::BindBool(int param_index, bool val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return BindInt64(param_index, val ? 1 : 0);
}

bool Statement::BindInt(int param_index, int val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  return sqlite3_bind_int(ref_->stmt(), param_index + 1, val) == SQLITE_OK;
}

bool Statement::BindInt64(int param_index, int64_t val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  return sqlite3_bind_int64(ref_->stmt(), param_index + 1, val) == SQLITE_OK;
}

bool Statement::BindDouble(int param_index, double val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  return sqlite3_bind_double(ref_->stmt(), param_index + 1, val) == SQLITE_OK;
}

bool Statement::BindTime(int param_index, base::Time val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  int64_t int_value = val.ToDeltaSinceWindowsEpoch().InMicroseconds();
  return sqlite3_bind_int64(ref_->stmt(), param_index + 1, int_value) ==
         SQLITE_OK;
}

bool Statement::BindCString(int param_index, const char* val) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  DCHECK(val);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";
  return sqlite3_bind_text(ref_->stmt(), param_index + 1, val, -1,
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Statement::BindString(int param_index, base::StringPiece value) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

  DCHECK_GE(param_index, 0);
  DCHECK_LT(param_index, sqlite3_bind_parameter_count(ref_->stmt()))
      << "Invalid parameter index";

  // base::StringPiece::data() may return null for empty pieces. In particular,
  // this may happen when the StringPiece is created from the default
  // constructor.
  //
  // However, sqlite3_bind_text() always interprets a nullptr data argument as a
  // NULL value, instead of an empty BLOB value.
  static constexpr char kEmptyPlaceholder[] = {0x00};
  const char* data = (value.size() > 0) ? value.data() : kEmptyPlaceholder;

  return sqlite3_bind_text(ref_->stmt(), param_index + 1, data, value.size(),
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Statement::BindString16(int param_index, base::StringPiece16 value) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return BindString(param_index, base::UTF16ToUTF8(value));
}

bool Statement::BindBlob(int param_index, base::span<const uint8_t> value) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID
  DCHECK(!stepped_);
  if (!is_valid())
    return false;

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

  return sqlite3_bind_blob(ref_->stmt(), param_index + 1, data, value.size(),
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

int Statement::ColumnCount() const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

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

ColumnType Statement::GetColumnType(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return static_cast<enum ColumnType>(sqlite3_column_type(ref_->stmt(), col));
}

bool Statement::ColumnBool(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return static_cast<bool>(ColumnInt(col));
}

int Statement::ColumnInt(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return 0;
  return sqlite3_column_int(ref_->stmt(), col);
}

int64_t Statement::ColumnInt64(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return 0;
  return sqlite3_column_int64(ref_->stmt(), col);
}

double Statement::ColumnDouble(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return 0;
  return sqlite3_column_double(ref_->stmt(), col);
}

base::Time Statement::ColumnTime(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return base::Time();

  int64_t int_value = sqlite3_column_int64(ref_->stmt(), col);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(int_value));
}

std::string Statement::ColumnString(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return std::string();

  const char* str = reinterpret_cast<const char*>(
      sqlite3_column_text(ref_->stmt(), col));
  int len = sqlite3_column_bytes(ref_->stmt(), col);

  std::string result;
  if (str && len > 0)
    result.assign(str, len);
  return result;
}

std::u16string Statement::ColumnString16(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return std::u16string();

  std::string s = ColumnString(col);
  return !s.empty() ? base::UTF8ToUTF16(s) : std::u16string();
}

int Statement::ColumnByteLength(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return 0;
  return sqlite3_column_bytes(ref_->stmt(), col);
}

const void* Statement::ColumnBlob(int col) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return nullptr;

  return sqlite3_column_blob(ref_->stmt(), col);
}

bool Statement::ColumnBlobAsString(int col, std::string* blob) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return false;

  const void* p = ColumnBlob(col);
  size_t len = ColumnByteLength(col);
  blob->resize(len);
  if (blob->size() != len) {
    return false;
  }
  blob->assign(reinterpret_cast<const char*>(p), len);
  return true;
}

bool Statement::ColumnBlobAsString16(int col, std::u16string* val) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  if (!CheckValid())
    return false;

  const void* data = ColumnBlob(col);
  size_t len = ColumnByteLength(col) / sizeof(char16_t);
  val->resize(len);
  if (val->size() != len)
    return false;
  val->assign(reinterpret_cast<const char16_t*>(data), len);
  return true;
}

bool Statement::ColumnBlobAsVector(int col, std::vector<char>* val) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  val->clear();

  if (!CheckValid())
    return false;

  const void* data = sqlite3_column_blob(ref_->stmt(), col);
  int len = sqlite3_column_bytes(ref_->stmt(), col);
  if (data && len > 0) {
    val->resize(len);
    memcpy(&(*val)[0], data, len);
  }
  return true;
}

bool Statement::ColumnBlobAsVector(
    int col,
    std::vector<unsigned char>* val) const {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return ColumnBlobAsVector(col, reinterpret_cast<std::vector<char>*>(val));
}

const char* Statement::GetSQLStatement() {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  return sqlite3_sql(ref_->stmt());
}

int Statement::CheckError(int err) {
#if !defined(OS_ANDROID)  // TODO(crbug.com/866218): Remove this conditional
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#endif  // OS_ANDROID

  // Please don't add DCHECKs here, OnSqliteError() already has them.
  succeeded_ = (err == SQLITE_OK || err == SQLITE_ROW || err == SQLITE_DONE);
  if (!succeeded_ && ref_.get() && ref_->database())
    return ref_->database()->OnSqliteError(err, this, nullptr);
  return err;
}

}  // namespace sql
