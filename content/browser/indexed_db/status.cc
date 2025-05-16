// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/status.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content::indexed_db {

Status::Status() : type_(Type::kOk), msg_("OK") {}

Status::Status(const Status& rhs) = default;

Status::Status(Status&& rhs) noexcept = default;

Status::Status(leveldb::Status&& status) noexcept
    : type_(Type::kDatabaseEngine), leveldb_status_(std::move(status)) {}

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
  return Status(Type::kIoError, msg.empty() ? "IO Error" : msg);
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
         (leveldb_status_ && leveldb_status_->IsCorruption());
}

bool Status::IsIOError() const {
  return type_ == Type::kIoError ||
         (leveldb_status_ && leveldb_status_->IsIOError());
}

bool Status::IsInvalidArgument() const {
  return type_ == Type::kInvalidArgument;
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

void Status::Log(std::string_view histogram_name) const {
  base::UmaHistogramEnumeration(
      histogram_name,
      static_cast<leveldb_env::LevelDBStatusValue>(GetTypeForLegacyLogging()),
      leveldb_env::LEVELDB_STATUS_MAX);
}

int Status::GetTypeForLegacyLogging() const {
  switch (type_) {
    case Type::kOk:
      return leveldb_env::LEVELDB_STATUS_OK;
    case Type::kNotFound:
      return leveldb_env::LEVELDB_STATUS_NOT_FOUND;
    case Type::kCorruption:
      return leveldb_env::LEVELDB_STATUS_CORRUPTION;
    case Type::kNotSupported:
      return leveldb_env::LEVELDB_STATUS_NOT_SUPPORTED;
    case Type::kInvalidArgument:
      return leveldb_env::LEVELDB_STATUS_INVALID_ARGUMENT;
    case Type::kIoError:
      return leveldb_env::LEVELDB_STATUS_IO_ERROR;
    case Type::kDatabaseEngine:
      return leveldb_env::GetLevelDBStatusUMAValue(*leveldb_status_);
  }
  NOTREACHED();
}

}  // namespace content::indexed_db
