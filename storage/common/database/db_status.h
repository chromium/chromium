// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATABASE_DB_STATUS_H_
#define STORAGE_COMMON_DATABASE_DB_STATUS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/types/expected.h"

namespace storage {

// A database status code and optionally an error message. This status code
// may have originated from the database engine or from the Chromium code. See
// notes above `type_`.
class COMPONENT_EXPORT(STORAGE_DATABASE_STATUS) DbStatus {
 public:
  DbStatus();
  DbStatus(const DbStatus& rhs);
  DbStatus(DbStatus&& rhs) noexcept;
  ~DbStatus();

  DbStatus& operator=(const DbStatus& rhs);
  DbStatus& operator=(DbStatus&&) noexcept;

  // Create a success or error status that didn't originate in the database
  // engine.
  static DbStatus OK();
  static DbStatus NotFound(std::string_view msg);
  static DbStatus Corruption(std::string_view msg);
  static DbStatus NotSupported(std::string_view msg);
  static DbStatus IOError(std::string_view msg = {});
  static DbStatus InvalidArgument(std::string_view msg);

  // Returns true iff the status indicates the corresponding success or error.
  bool ok() const;
  bool IsNotFound() const;
  bool IsCorruption() const;
  bool IsNotSupported() const;
  bool IsIOError() const;
  bool IsInvalidArgument() const;

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

 private:
  enum class Type {
    kOk = 0,

    // Something wasn't found.
    kNotFound,

    // The database is in an inconsistent state.
    kCorruption,

    kNotSupported,

    // Generally speaking, indicates a programming error or unexpected state in
    // Chromium. For example, an invalid object store ID is sent as a parameter
    // over IPC.
    kInvalidArgument,

    // Possibly transient read or write error.
    kIoError,
  };

  DbStatus(Type type, std::string_view msg);

  // The specific type of error. Note that the treatment of this is quite
  // inconsistent:
  // * sometimes it has semantic value, as in code branches based on
  //   `IsCorruption()`
  // * sometimes it's used for logging
  // * sometimes it's just ignored
  Type type_;

  std::string msg_;
};

// Makes a common return value more concise. For this return type, "no error" is
// represented by returning a value for `T`, and the DbStatus should never be
// `ok()`.
template <typename T>
using StatusOr = base::expected<T, DbStatus>;

// One common way of returning an error from a function that does not otherwise
// return a value would be base::expected<void, DbStatus>, and that would allow
// us to make use of the `base::expected` macros such as RETURN_IF_ERROR.
// However, that would require updating tons of code, so we simply define
// similar macros.
#define IDB_RETURN_IF_ERROR_AND_DO(expr, on_error) \
  {                                                \
    DbStatus _status = expr;                       \
    if (!_status.ok()) [[unlikely]] {              \
      on_error;                                    \
      return _status;                              \
    }                                              \
  }

#define IDB_RETURN_IF_ERROR(expr) IDB_RETURN_IF_ERROR_AND_DO(expr, {})

}  // namespace storage

#endif  // STORAGE_COMMON_DATABASE_DB_STATUS_H_
