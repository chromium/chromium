// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/status.h"

#include <string>

namespace content::indexed_db {

Status::Status() noexcept = default;

Status::Status(const Status& rhs) noexcept = default;

Status::Status(const leveldb::Status& status) noexcept : status_(status) {}

Status::~Status() = default;

Status& Status::operator=(const Status& rhs) = default;

Status& Status::operator=(const leveldb::Status& rhs) {
  status_ = rhs;
  return *this;
}

// static
Status Status::OK() {
  return Status();
}

// static
Status Status::NotFound(const std::string& msg, const std::string& msg2) {
  return Status(leveldb::Status::NotFound(msg, msg2));
}

// static
Status Status::Corruption(const std::string& msg, const std::string& msg2) {
  return Status(leveldb::Status::Corruption(msg, msg2));
}

// static
Status Status::NotSupported(const std::string& msg, const std::string& msg2) {
  return Status(leveldb::Status::NotSupported(msg, msg2));
}

// static
Status Status::InvalidArgument(const std::string& msg,
                               const std::string& msg2) {
  return Status(leveldb::Status::InvalidArgument(msg, msg2));
}

// static
Status Status::IOError(const std::string& msg, const std::string& msg2) {
  return Status(leveldb::Status::IOError(msg, msg2));
}

bool Status::IsNotFound() const {
  return status_ && status_->IsNotFound();
}

bool Status::IsCorruption() const {
  return status_ && status_->IsCorruption();
}

bool Status::IsIOError() const {
  return status_ && status_->IsIOError();
}

bool Status::IsNotSupportedError() const {
  return status_ && status_->IsNotSupportedError();
}

bool Status::IsInvalidArgument() const {
  return status_ && status_->IsInvalidArgument();
}

std::string Status::ToString() const {
  return status_ ? status_->ToString() : "OK";
}

}  // namespace content::indexed_db
