// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_STATUS_H_
#define CONTENT_BROWSER_INDEXED_DB_STATUS_H_

#include <optional>
#include <string>

#include "base/strings/string_util.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content::indexed_db {

// A backing store status code and message.
// This thin leveldb::Status wrapper aids SQLite backing store prototyping.
class CONTENT_EXPORT Status {
 public:
  Status() noexcept;
  Status(const Status& rhs) noexcept;
  explicit Status(const leveldb::Status& status) noexcept;
  ~Status();

  Status& operator=(const Status& rhs);
  Status& operator=(const leveldb::Status& rhs);

  // Create a corresponding success or error status.
  static Status OK();
  static Status NotFound(const std::string& msg,
                         const std::string& msg2 = base::EmptyString());
  static Status Corruption(const std::string& msg,
                           const std::string& msg2 = base::EmptyString());
  static Status NotSupported(const std::string& msg,
                             const std::string& msg2 = base::EmptyString());
  static Status InvalidArgument(const std::string& msg,
                                const std::string& msg2 = base::EmptyString());
  static Status IOError(const std::string& msg,
                        const std::string& msg2 = base::EmptyString());

  // Returns true iff the status indicates the corresponding success or error.
  bool ok() const { return !status_ || status_->ok(); }
  bool IsNotFound() const;
  bool IsCorruption() const;
  bool IsIOError() const;
  bool IsNotSupportedError() const;
  bool IsInvalidArgument() const;

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

  leveldb::Status leveldb_status() const {
    return status_.value_or(leveldb::Status::OK());
  }

 private:
  std::optional<leveldb::Status> status_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_STATUS_H_
