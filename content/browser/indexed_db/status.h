// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_STATUS_H_
#define CONTENT_BROWSER_INDEXED_DB_STATUS_H_

#include <optional>
#include <string>

#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "sql/sqlite_result_code.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace sql {
class Database;
}

namespace content::indexed_db {

// A backing store status code and optionally an error message. This status code
// may have originated from the database engine or from the Chromium code. See
// notes above `type_`.
class CONTENT_EXPORT Status {
 public:
  Status();
  Status(const Status& rhs);
  Status(Status&& rhs) noexcept;
  // Wraps the given LevelDB status.
  Status(leveldb::Status&& rhs) noexcept;
  // Wraps the SQLite error code and message associated with the last operation
  // on `db`.
  explicit Status(sql::Database& db) noexcept;
  ~Status();

  Status& operator=(const Status& rhs);
  Status& operator=(Status&&) noexcept;
  Status& operator=(leveldb::Status&& rhs) noexcept;

  // Create a success or error status that didn't originate in the database
  // engine.
  [[nodiscard]] static Status OK();
  [[nodiscard]] static Status InvalidArgument(std::string_view msg);
  [[nodiscard]] static Status NotFound(std::string_view msg);
  [[nodiscard]] static Status IOError(std::string_view msg = {});
  [[nodiscard]] static Status Corruption(std::string_view msg);

  // Returns true iff the status indicates the corresponding success or error.
  bool ok() const;
  bool IsNotFound() const;
  bool IsCorruption() const;
  bool IsIOError() const;
  bool IsInvalidArgument() const;

  // Logs the `Type` of `this` to `histogram_name`.
  void Log(std::string_view histogram_name) const;

  // Return a string representation of this status suitable for printing.
  // Returns the string "OK" for success.
  std::string ToString() const;

  const std::optional<leveldb::Status>& leveldb_status() const {
    return leveldb_status_;
  }

  const std::optional<sql::SqliteResultCode>& sqlite_code() const {
    return sqlite_code_;
  }

  // Legacy methods applicable only to the LevelDB backing store.
  bool IndicatesDiskFull() const;
  void LogLevelDbStatus(std::string_view histogram_name) const;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(Type)
  enum class Type {
    kOk = 0,

    // Something wasn't found.
    kNotFound = 1,

    // The database is in an inconsistent state.
    kCorruption = 2,

    // Generally speaking, indicates a programming error or unexpected state in
    // Chromium. For example, an invalid object store ID is sent as a parameter
    // over IPC.
    kInvalidArgument = 3,

    // Possibly transient read or write error.
    kIoError = 4,

    // An error reported by the database engine, e.g. LevelDB.
    kDatabaseEngine = 5,

    kMaxValue = kDatabaseEngine,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:IndexedDbStatusType)

  Status(Type type, std::string_view msg);

  // The specific type of error. Note that the treatment of this is quite
  // inconsistent:
  // * sometimes it has semantic value, as in code branches based on
  //   `IsCorruption()`
  // * sometimes it's used for logging
  // * sometimes it's just ignored
  // * a single error can be semantically more than one type, e.g. a piece of
  //   metadata being "not found" could indicate "corruption".
  // * helpers like `IsCorruption()` might return true for kCorruption or
  //   kDatabaseEngine type errors.
  //
  // This is too hard to clean up for legacy code, but should be improved upon
  // in the future, i.e. with the SQLite backend.
  Type type_;

  // The LevelDB status and SQLite result code are mutually exclusive, and both
  // may be null if the error originated in engine-agnostic Chromium code.
  std::optional<leveldb::Status> leveldb_status_;
  std::optional<sql::SqliteResultCode> sqlite_code_;
  std::string msg_;
};

// Makes a common return value more concise. For this return type, "no error" is
// represented by returning a value for `T`, and the Status should never be
// `ok()`.
template <typename T>
using StatusOr = base::expected<T, Status>;

// One common way of returning an error from a function that does not otherwise
// return a value would be base::expected<void, Status>, and that would allow us
// to make use of the `base::expected` macros such as RETURN_IF_ERROR. However,
// that would require updating tons of code, so we simply define similar macros.
#define IDB_RETURN_IF_ERROR_AND_DO(expr, on_error) \
  {                                                \
    Status _status = expr;                         \
    if (!_status.ok()) [[unlikely]] {              \
      on_error;                                    \
      return _status;                              \
    }                                              \
  }

#define IDB_RETURN_IF_ERROR(expr) IDB_RETURN_IF_ERROR_AND_DO(expr, {})

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_STATUS_H_
