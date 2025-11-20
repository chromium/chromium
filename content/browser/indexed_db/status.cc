// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/status.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/sqlite_result_code.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content::indexed_db {

Status::Status() : type_(Type::kOk), msg_("OK") {}

Status::Status(const Status& rhs) = default;

Status::Status(Status&& rhs) noexcept = default;

Status::Status(leveldb::Status&& status) noexcept
    : type_(status.ok() ? Type::kOk : Type::kDatabaseEngine),
      leveldb_status_(std::move(status)) {}

Status::Status(sql::Database& db) noexcept
    : type_(Type::kDatabaseEngine),
      sqlite_code_(sql::ToSqliteResultCode(db.GetErrorCode())),
      msg_(db.GetErrorMessage()) {}

Status::Status(Status::Type type, std::string_view msg)
    : type_(type), msg_(msg) {
  CHECK(!msg_.empty());
}

Status::~Status() = default;

Status& Status::operator=(const Status& rhs) = default;

Status& Status::operator=(Status&& rhs) noexcept = default;

Status& Status::operator=(leveldb::Status&& rhs) noexcept {
  msg_.clear();
  leveldb_status_ = std::move(rhs);
  type_ = Type::kDatabaseEngine;
  return *this;
}

// static
Status Status::OK() {
  return Status();
}

// static
Status Status::NotFound(std::string_view msg) {
  return Status(Type::kNotFound, msg);
}

// static
Status Status::Corruption(std::string_view msg) {
  return Status(Type::kCorruption, msg);
}

// static
Status Status::InvalidArgument(std::string_view msg) {
  return Status(Type::kInvalidArgument, msg);
}

// static
Status Status::IOError(std::string_view msg) {
  return Status(Type::kIoError, msg);
}

bool Status::ok() const {
  return type_ == Type::kOk || (leveldb_status_ && leveldb_status_->ok());
}

bool Status::IsNotFound() const {
  return type_ == Type::kNotFound ||
         (leveldb_status_ && leveldb_status_->IsNotFound());
}

bool Status::IsCorruption() const {
  return type_ == Type::kCorruption ||
         (leveldb_status_ && leveldb_status_->IsCorruption()) ||
         (sqlite_code_ &&
          sql::IsErrorCatastrophic(static_cast<int>(sqlite_code_.value())));
}

bool Status::IsIOError() const {
  return type_ == Type::kIoError ||
         (leveldb_status_ && leveldb_status_->IsIOError()) ||
         sqlite_code_ == sql::SqliteResultCode::kIo ||
         sqlite_code_ == sql::SqliteResultCode::kError;
}

bool Status::IsInvalidArgument() const {
  return type_ == Type::kInvalidArgument;
}

void Status::Log(std::string_view histogram_name) const {
  base::UmaHistogramEnumeration(histogram_name, type_);
}

std::string Status::ToString() const {
  if (leveldb_status_) {
    return leveldb_status_->ToString();
  }
  return msg_;
}

bool Status::IndicatesDiskFull() const {
  return leveldb_status_ && leveldb_env::IndicatesDiskFull(*leveldb_status_);
}

void Status::LogLevelDbStatus(std::string_view histogram_name) const {
  leveldb_env::LevelDBStatusValue value;
  switch (type_) {
    case Type::kOk:
      value = leveldb_env::LEVELDB_STATUS_OK;
      break;
    case Type::kNotFound:
      value = leveldb_env::LEVELDB_STATUS_NOT_FOUND;
      break;
    case Type::kCorruption:
      value = leveldb_env::LEVELDB_STATUS_CORRUPTION;
      break;
    case Type::kInvalidArgument:
      value = leveldb_env::LEVELDB_STATUS_INVALID_ARGUMENT;
      break;
    case Type::kIoError:
      value = leveldb_env::LEVELDB_STATUS_IO_ERROR;
      break;
    case Type::kDatabaseEngine:
      value = leveldb_env::GetLevelDBStatusUMAValue(*leveldb_status_);
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, value,
                                leveldb_env::LEVELDB_STATUS_MAX);
}

}  // namespace content::indexed_db
