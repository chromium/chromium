// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// This empty constructor initializes our reference with an empty one so that
// we don't have to null-check the ref_ to see if the statement is valid: we
// only have to check the ref's validity bit.
Statement::Statement()
    : ref_(base::MakeRefCounted<Database::StatementRef>(nullptr,
                                                        nullptr,
                                                        false)),
      stepped_(false),
      succeeded_(false) {}

Statement::Statement(scoped_refptr<Database::StatementRef> ref)
    : ref_(std::move(ref)), stepped_(false), succeeded_(false) {}

Statement::~Statement() {
  // Free the resources associated with this statement. We assume there's only
  // one statement active for a given sqlite3_stmt at any time, so this won't
  // mess with anything.
  Reset(true);
}

void Statement::Assign(scoped_refptr<Database::StatementRef> ref) {
  Reset(true);
  ref_ = std::move(ref);
}

void Statement::Clear() {
  Assign(base::MakeRefCounted<Database::StatementRef>(nullptr, nullptr, false));
  succeeded_ = false;
}

bool Statement::CheckValid() const {
  // Allow operations to fail silently if a statement was invalidated
  // because the database was closed by an error handler.
  DLOG_IF(FATAL, !ref_->was_valid())
      << "Cannot call mutating statements on an invalid statement.";
  return is_valid();
}

int Statement::StepInternal() {
  if (!CheckValid())
    return SQLITE_ERROR;

  base::Optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(&scoped_blocking_call);

  stepped_ = true;
  int ret = sqlite3_step(ref_->stmt());
  return CheckError(ret);
}

bool Statement::Run() {
  DCHECK(!stepped_);
  return StepInternal() == SQLITE_DONE;
}

bool Statement::Step() {
  return StepInternal() == SQLITE_ROW;
}

void Statement::Reset(bool clear_bound_vars) {
  base::Optional<base::ScopedBlockingCall> scoped_blocking_call;
  ref_->InitScopedBlockingCall(&scoped_blocking_call);
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
  return is_valid() && succeeded_;
}

bool Statement::BindNull(int col) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_null(ref_->stmt(), col + 1));
}

bool Statement::BindBool(int col, bool val) {
  return BindInt(col, val ? 1 : 0);
}

bool Statement::BindInt(int col, int val) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_int(ref_->stmt(), col + 1, val));
}

bool Statement::BindInt64(int col, int64_t val) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_int64(ref_->stmt(), col + 1, val));
}

bool Statement::BindDouble(int col, double val) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_double(ref_->stmt(), col + 1, val));
}

bool Statement::BindCString(int col, const char* val) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_text(ref_->stmt(), col + 1, val, -1,
                                                 SQLITE_TRANSIENT));
}

bool Statement::BindString(int col, const std::string& val) {
  DCHECK(!stepped_);

  return is_valid() &&
         CheckOk(sqlite3_bind_text(ref_->stmt(), col + 1, val.data(),
                                   base::checked_cast<int>(val.size()),
                                   SQLITE_TRANSIENT));
}

bool Statement::BindString16(int col, const base::string16& value) {
  return BindString(col, base::UTF16ToUTF8(value));
}

bool Statement::BindBlob(int col, const void* val, int val_len) {
  DCHECK(!stepped_);

  return is_valid() && CheckOk(sqlite3_bind_blob(ref_->stmt(), col + 1, val,
                                                 val_len, SQLITE_TRANSIENT));
}

int Statement::ColumnCount() const {
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
  return static_cast<enum ColumnType>(sqlite3_column_type(ref_->stmt(), col));
}

bool Statement::ColumnBool(int col) const {
  return static_cast<bool>(ColumnInt(col));
}

int Statement::ColumnInt(int col) const {
  if (!CheckValid())
    return 0;
  return sqlite3_column_int(ref_->stmt(), col);
}

int64_t Statement::ColumnInt64(int col) const {
  if (!CheckValid())
    return 0;
  return sqlite3_column_int64(ref_->stmt(), col);
}

double Statement::ColumnDouble(int col) const {
  if (!CheckValid())
    return 0;
  return sqlite3_column_double(ref_->stmt(), col);
}

std::string Statement::ColumnString(int col) const {
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

base::string16 Statement::ColumnString16(int col) const {
  if (!CheckValid())
    return base::string16();

  std::string s = ColumnString(col);
  return !s.empty() ? base::UTF8ToUTF16(s) : base::string16();
}

int Statement::ColumnByteLength(int col) const {
  if (!CheckValid())
    return 0;
  return sqlite3_column_bytes(ref_->stmt(), col);
}

const void* Statement::ColumnBlob(int col) const {
  if (!CheckValid())
    return nullptr;

  return sqlite3_column_blob(ref_->stmt(), col);
}

bool Statement::ColumnBlobAsString(int col, std::string* blob) const {
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

bool Statement::ColumnBlobAsString16(int col, base::string16* val) const {
  if (!CheckValid())
    return false;

  const void* data = ColumnBlob(col);
  size_t len = ColumnByteLength(col) / sizeof(base::char16);
  val->resize(len);
  if (val->size() != len)
    return false;
  val->assign(reinterpret_cast<const base::char16*>(data), len);
  return true;
}

bool Statement::ColumnBlobAsVector(int col, std::vector<char>* val) const {
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
  return ColumnBlobAsVector(col, reinterpret_cast< std::vector<char>* >(val));
}

const char* Statement::GetSQLStatement() {
  return sqlite3_sql(ref_->stmt());
}

bool Statement::CheckOk(int err) const {
  // Binding to a non-existent variable is evidence of a serious error.
  // TODO(gbillock,shess): make this invalidate the statement so it
  // can't wreak havoc.
  DCHECK_NE(err, SQLITE_RANGE) << "Bind value out of range";
  return err == SQLITE_OK;
}

int Statement::CheckError(int err) {
  // Please don't add DCHECKs here, OnSqliteError() already has them.
  succeeded_ = (err == SQLITE_OK || err == SQLITE_ROW || err == SQLITE_DONE);
  if (!succeeded_ && ref_.get() && ref_->database())
    return ref_->database()->OnSqliteError(err, this, nullptr);
  return err;
}

}  // namespace sql
