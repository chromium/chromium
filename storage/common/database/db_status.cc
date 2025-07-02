// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/database/db_status.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace storage {

DbStatus::DbStatus() : type_(Type::kOk), msg_("OK") {}

DbStatus::DbStatus(const DbStatus& rhs) = default;

DbStatus::DbStatus(DbStatus&& rhs) noexcept = default;

DbStatus::DbStatus(DbStatus::Type type, std::string_view msg)
    : type_(type), msg_(msg) {
  CHECK(!msg_.empty());
}

DbStatus::~DbStatus() = default;

DbStatus& DbStatus::operator=(const DbStatus& rhs) = default;

DbStatus& DbStatus::operator=(DbStatus&& rhs) noexcept = default;

// static
DbStatus DbStatus::OK() {
  return DbStatus();
}

// static
DbStatus DbStatus::NotFound(std::string_view msg) {
  return DbStatus(Type::kNotFound, msg);
}

// static
DbStatus DbStatus::Corruption(std::string_view msg) {
  return DbStatus(Type::kCorruption, msg);
}

// static
DbStatus DbStatus::NotSupported(std::string_view msg) {
  return DbStatus(Type::kNotSupported, msg);
}

// static
DbStatus DbStatus::InvalidArgument(std::string_view msg) {
  return DbStatus(Type::kInvalidArgument, msg);
}

// static
DbStatus DbStatus::IOError(std::string_view msg) {
  return DbStatus(Type::kIoError, msg);
}

bool DbStatus::ok() const {
  return type_ == Type::kOk;
}

bool DbStatus::IsNotFound() const {
  return type_ == Type::kNotFound;
}

bool DbStatus::IsCorruption() const {
  return type_ == Type::kCorruption;
}

bool DbStatus::IsNotSupported() const {
  return type_ == Type::kNotSupported;
}

bool DbStatus::IsIOError() const {
  return type_ == Type::kIoError;
}

bool DbStatus::IsInvalidArgument() const {
  return type_ == Type::kInvalidArgument;
}

std::string DbStatus::ToString() const {
  return msg_;
}

}  // namespace storage
