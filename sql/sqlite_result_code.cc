// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sqlite_result_code.h"

#include <cstddef>
#include <ostream>  // Needed to compile CHECK() with operator <<.
#include <ranges>
#include <set>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "sql/sqlite_result_code_values.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

// The highly packed representation minimizes binary size and memory usage.
struct SqliteResultCodeMappingEntry {
  unsigned result_code : 16;
  unsigned logged_code : 8;

  // The remaining bits will be used to encode the result values of helpers that
  // indicate corruption handling.
};

constexpr SqliteResultCodeMappingEntry kResultCodeMapping[] = {
    // Entries are ordered by SQLite result code value. This should match the
    // ordering in https://www.sqlite.org/rescode.html.

    {SQLITE_OK, static_cast<int>(SqliteLoggedResultCode::kNoError)},
    {SQLITE_ERROR, static_cast<int>(SqliteLoggedResultCode::kGeneric)},
    {SQLITE_INTERNAL, static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_PERM, static_cast<int>(SqliteLoggedResultCode::kPermission)},
    {SQLITE_ABORT, static_cast<int>(SqliteLoggedResultCode::kAbort)},
    {SQLITE_BUSY, static_cast<int>(SqliteLoggedResultCode::kBusy)},

    // Chrome features shouldn't execute conflicting statements concurrently.
    {SQLITE_LOCKED, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // Chrome should crash on OOM.
    {SQLITE_NOMEM, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_READONLY, static_cast<int>(SqliteLoggedResultCode::kReadOnly)},

    // Chrome doesn't use sqlite3_interrupt().
    {SQLITE_INTERRUPT, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR, static_cast<int>(SqliteLoggedResultCode::kIo)},
    {SQLITE_CORRUPT, static_cast<int>(SqliteLoggedResultCode::kCorrupt)},

    // Chrome only passes a few known-good opcodes to sqlite3_file_control().
    {SQLITE_NOTFOUND, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_FULL, static_cast<int>(SqliteLoggedResultCode::kFullDisk)},
    {SQLITE_CANTOPEN, static_cast<int>(SqliteLoggedResultCode::kCantOpen)},
    {SQLITE_PROTOCOL,
     static_cast<int>(SqliteLoggedResultCode::kLockingProtocol)},
    {SQLITE_EMPTY, static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_SCHEMA, static_cast<int>(SqliteLoggedResultCode::kSchemaChanged)},
    {SQLITE_TOOBIG, static_cast<int>(SqliteLoggedResultCode::kTooBig)},
    {SQLITE_CONSTRAINT, static_cast<int>(SqliteLoggedResultCode::kConstraint)},
    {SQLITE_MISMATCH, static_cast<int>(SqliteLoggedResultCode::kTypeMismatch)},

    // Chrome should not misuse SQLite's API.
    {SQLITE_MISUSE, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_NOLFS,
     static_cast<int>(SqliteLoggedResultCode::kNoLargeFileSupport)},

    // Chrome does not set an authorizer callback.
    {SQLITE_AUTH, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_FORMAT, static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome should not use invalid column indexes in sqlite3_{bind,column}*().
    {SQLITE_RANGE, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_NOTADB, static_cast<int>(SqliteLoggedResultCode::kNotADatabase)},
    {SQLITE_NOTICE, static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_WARNING, static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_ROW, static_cast<int>(SqliteLoggedResultCode::kNoError)},
    {SQLITE_DONE, static_cast<int>(SqliteLoggedResultCode::kNoError)},
    {SQLITE_OK_LOAD_PERMANENTLY,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome should not use collating sequence names in SQL statements.
    {SQLITE_ERROR_MISSING_COLLSEQ,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_BUSY_RECOVERY,
     static_cast<int>(SqliteLoggedResultCode::kBusyRecovery)},

    // Chrome does not use a shared page cache.
    {SQLITE_LOCKED_SHAREDCACHE,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_READONLY_RECOVERY,
     static_cast<int>(SqliteLoggedResultCode::kReadOnlyRecovery)},
    {SQLITE_IOERR_READ, static_cast<int>(SqliteLoggedResultCode::kIoRead)},

    // Chrome does not use a virtual table that signals corruption. We only use
    // a
    // virtual table code for recovery. That code does not use this error.
    {SQLITE_CORRUPT_VTAB,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_CANTOPEN_NOTEMPDIR,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_CONSTRAINT_CHECK,
     static_cast<int>(SqliteLoggedResultCode::kConstraintCheck)},

    // Chrome does not set an authorizer callback.
    {SQLITE_AUTH_USER, static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_NOTICE_RECOVER_WAL,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_WARNING_AUTOINDEX,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_ERROR_RETRY,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_ABORT_ROLLBACK,
     static_cast<int>(SqliteLoggedResultCode::kAbortRollback)},
    {SQLITE_BUSY_SNAPSHOT,
     static_cast<int>(SqliteLoggedResultCode::kBusySnapshot)},

    // Chrome does not use a virtual table that signals conflicts. We only use a
    // virtual table code for recovery. That code does not use this error.
    {SQLITE_LOCKED_VTAB,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_READONLY_CANTLOCK,
     static_cast<int>(SqliteLoggedResultCode::kReadOnlyCantLock)},
    {SQLITE_IOERR_SHORT_READ,
     static_cast<int>(SqliteLoggedResultCode::kIoShortRead)},
    {SQLITE_CORRUPT_SEQUENCE,
     static_cast<int>(SqliteLoggedResultCode::kCorruptSequence)},
    {SQLITE_CANTOPEN_ISDIR,
     static_cast<int>(SqliteLoggedResultCode::kCantOpenIsDir)},

    // Chrome does not use commit hook callbacks.
    {SQLITE_CONSTRAINT_COMMITHOOK,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_NOTICE_RECOVER_ROLLBACK,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome does not use sqlite3_snapshot_open().
    {SQLITE_ERROR_SNAPSHOT,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},
#ifdef SQLITE_ENABLE_SNAPSHOT
#error "This code assumes that Chrome does not use sqlite3_snapshot_open()"
#endif

    // Chrome does not use blocking Posix advisory file lock requests.
    {SQLITE_BUSY_TIMEOUT,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},
#ifdef SQLITE_ENABLE_SETLK_TIMEOUT
#error \
    "This code assumes that Chrome does not use blocking Posix advisory \
file lock requests"
#endif

    {SQLITE_READONLY_ROLLBACK,
     static_cast<int>(SqliteLoggedResultCode::kReadOnlyRollback)},
    {SQLITE_IOERR_WRITE, static_cast<int>(SqliteLoggedResultCode::kIoWrite)},
    {SQLITE_CORRUPT_INDEX,
     static_cast<int>(SqliteLoggedResultCode::kCorruptIndex)},

    // Chrome should always pass full paths to SQLite.
    {SQLITE_CANTOPEN_FULLPATH,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_CONSTRAINT_FOREIGNKEY,
     static_cast<int>(SqliteLoggedResultCode::kConstraintForeignKey)},
    {SQLITE_READONLY_DBMOVED,
     static_cast<int>(SqliteLoggedResultCode::kReadOnlyDbMoved)},
    {SQLITE_IOERR_FSYNC, static_cast<int>(SqliteLoggedResultCode::kIoFsync)},

    // Chrome does not support Cygwin and does not use its VFS.
    {SQLITE_CANTOPEN_CONVPATH,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // Chrome does not use extension functions.
    {SQLITE_CONSTRAINT_FUNCTION,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_READONLY_CANTINIT,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_IOERR_DIR_FSYNC,
     static_cast<int>(SqliteLoggedResultCode::kIoDirFsync)},
    {SQLITE_CANTOPEN_DIRTYWAL,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},
    {SQLITE_CONSTRAINT_NOTNULL,
     static_cast<int>(SqliteLoggedResultCode::kConstraintNotNull)},
    {SQLITE_READONLY_DIRECTORY,
     static_cast<int>(SqliteLoggedResultCode::kReadOnlyDirectory)},
    {SQLITE_IOERR_TRUNCATE,
     static_cast<int>(SqliteLoggedResultCode::kIoTruncate)},

    // Chrome does not use the SQLITE_OPEN_NOFOLLOW flag.
    {SQLITE_CANTOPEN_SYMLINK,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_CONSTRAINT_PRIMARYKEY,
     static_cast<int>(SqliteLoggedResultCode::kConstraintPrimaryKey)},
    {SQLITE_IOERR_FSTAT, static_cast<int>(SqliteLoggedResultCode::kIoFstat)},

    // Chrome unconditionally disables database triggers via
    // sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_TRIGGER).
    {SQLITE_CONSTRAINT_TRIGGER,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_UNLOCK, static_cast<int>(SqliteLoggedResultCode::kIoUnlock)},
    {SQLITE_CONSTRAINT_UNIQUE,
     static_cast<int>(SqliteLoggedResultCode::kConstraintUnique)},
    {SQLITE_IOERR_RDLOCK,
     static_cast<int>(SqliteLoggedResultCode::kIoReadLock)},

    // Chrome does not use a virtual table that signals constraints. We only use
    // a virtual table code for recovery. That code does not use this error.
    {SQLITE_CONSTRAINT_VTAB,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_DELETE, static_cast<int>(SqliteLoggedResultCode::kIoDelete)},
    {SQLITE_CONSTRAINT_ROWID,
     static_cast<int>(SqliteLoggedResultCode::kConstraintRowId)},
    {SQLITE_IOERR_BLOCKED,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome unconditionally disables database triggers via
    // sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_TRIGGER).
    {SQLITE_CONSTRAINT_PINNED,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // The SQLite docus claim that this error code is "normally" converted to
    // SQLITE_NOMEM. This doesn't seem 100% categorical, so we're flagging this
    // as "unused in Chrome" per the same rationale as SQLITE_NOMEM.
    {SQLITE_IOERR_NOMEM,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_CONSTRAINT_DATATYPE,
     static_cast<int>(SqliteLoggedResultCode::kConstraintDataType)},
    {SQLITE_IOERR_ACCESS, static_cast<int>(SqliteLoggedResultCode::kIoAccess)},
    {SQLITE_IOERR_CHECKRESERVEDLOCK,
     static_cast<int>(SqliteLoggedResultCode::kIoCheckReservedLock)},
    {SQLITE_IOERR_LOCK, static_cast<int>(SqliteLoggedResultCode::kIoLock)},
    {SQLITE_IOERR_CLOSE, static_cast<int>(SqliteLoggedResultCode::kIoClose)},
    {SQLITE_IOERR_DIR_CLOSE,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMOPEN,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMSIZE,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_SHMLOCK,
     static_cast<int>(SqliteLoggedResultCode::kUnusedSqlite)},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMMAP,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_SEEK, static_cast<int>(SqliteLoggedResultCode::kIoSeek)},
    {SQLITE_IOERR_DELETE_NOENT,
     static_cast<int>(SqliteLoggedResultCode::kIoDeleteNoEntry)},
    {SQLITE_IOERR_MMAP,
     static_cast<int>(SqliteLoggedResultCode::kIoMemoryMapping)},
    {SQLITE_IOERR_GETTEMPPATH,
     static_cast<int>(SqliteLoggedResultCode::kIoGetTemporaryPath)},

    // Chrome does not support Cygwin and does not use its VFS.
    {SQLITE_IOERR_CONVPATH,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // Chrome does not use SQLite extensions.
    {SQLITE_IOERR_VNODE,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    // Chrome does not use SQLite extensions.
    {SQLITE_IOERR_AUTH,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_BEGIN_ATOMIC,
     static_cast<int>(SqliteLoggedResultCode::kIoBeginAtomic)},
    {SQLITE_IOERR_COMMIT_ATOMIC,
     static_cast<int>(SqliteLoggedResultCode::kIoCommitAtomic)},
    {SQLITE_IOERR_ROLLBACK_ATOMIC,
     static_cast<int>(SqliteLoggedResultCode::kIoRollbackAtomic)},

    // Chrome does not use the checksum VFS shim.
    {SQLITE_IOERR_DATA,
     static_cast<int>(SqliteLoggedResultCode::kUnusedChrome)},

    {SQLITE_IOERR_CORRUPTFS,
     static_cast<int>(SqliteLoggedResultCode::kIoCorruptFileSystem)},
};

// Looks up a `sqlite_result_code` in the mapping tables.
//
// Returns an entry in kResultCodeMapping or kUnknownResultCodeMappingEntry.
// CHECKs if the `sqlite_result_code` is not in the mapping table.
SqliteResultCodeMappingEntry FindResultCode(int sqlite_result_code) {
  const auto* mapping_it = base::ranges::find_if(
      kResultCodeMapping,
      [&sqlite_result_code](SqliteResultCodeMappingEntry rhs) {
        return sqlite_result_code == rhs.result_code;
      });

  CHECK(mapping_it != std::ranges::end(kResultCodeMapping))
      << "Unsupported SQLite result code: " << sqlite_result_code;
  return *mapping_it;
}

}  // namespace

#if DCHECK_IS_ON()

SqliteResultCode ToSqliteResultCode(int sqlite_result_code) {
  SqliteLoggedResultCode logged_code = static_cast<SqliteLoggedResultCode>(
      FindResultCode(sqlite_result_code).logged_code);

  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedSqlite)
      << "SQLite reported code marked for internal use: " << sqlite_result_code;
  DVLOG_IF(1, logged_code == SqliteLoggedResultCode::kUnusedChrome)
      << "SQLite reported code that should never show up in Chrome unless a "
         "sql database has been corrupted: "
      << sqlite_result_code;

  return static_cast<SqliteResultCode>(sqlite_result_code);
}

SqliteErrorCode ToSqliteErrorCode(SqliteResultCode sqlite_error_code) {
  SqliteLoggedResultCode logged_code = static_cast<SqliteLoggedResultCode>(
      FindResultCode(static_cast<int>(sqlite_error_code)).logged_code);

  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedSqlite)
      << "SQLite reported code marked for internal use: " << sqlite_error_code;
  DVLOG_IF(1, logged_code == SqliteLoggedResultCode::kUnusedChrome)
      << "SQLite reported code that should never show up in Chrome unless a "
         "sql database has been corrupted: "
      << sqlite_error_code;
  DCHECK_NE(logged_code, SqliteLoggedResultCode::kNoError)
      << __func__
      << " called with non-error result code: " << sqlite_error_code;

  return static_cast<SqliteErrorCode>(sqlite_error_code);
}

#endif  // DCHECK_IS_ON()

bool IsSqliteSuccessCode(SqliteResultCode sqlite_result_code) {
  // https://www.sqlite.org/rescode.html lists the result codes that are not
  // errors.
  bool is_success = (sqlite_result_code == SqliteResultCode::kOk) ||
                    (sqlite_result_code == SqliteResultCode::kRow) ||
                    (sqlite_result_code == SqliteResultCode::kDone);

#if DCHECK_IS_ON()
  SqliteLoggedResultCode logged_code = static_cast<SqliteLoggedResultCode>(
      FindResultCode(static_cast<int>(sqlite_result_code)).logged_code);

  DCHECK_EQ(is_success, logged_code == SqliteLoggedResultCode::kNoError)
      << __func__ << " logic disagrees with the code mapping for "
      << sqlite_result_code;

  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedSqlite)
      << "SQLite reported code marked for internal use: " << sqlite_result_code;
  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedChrome)
      << "SQLite reported code that should never show up in Chrome: "
      << sqlite_result_code;
#endif  // DCHECK_IS_ON()

  return is_success;
}

SqliteLoggedResultCode ToSqliteLoggedResultCode(int sqlite_result_code) {
  SqliteLoggedResultCode logged_code = static_cast<SqliteLoggedResultCode>(
      FindResultCode(sqlite_result_code).logged_code);

  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedSqlite)
      << "SQLite reported code marked for internal use: " << sqlite_result_code;
  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedChrome)
      << "SQLite reported code that should never show up in Chrome: "
      << sqlite_result_code;
  return logged_code;
}

void UmaHistogramSqliteResult(const std::string& histogram_name,
                              int sqlite_result_code) {
  auto logged_code = ToSqliteLoggedResultCode(sqlite_result_code);
  base::UmaHistogramEnumeration(histogram_name, logged_code);
}

std::ostream& operator<<(std::ostream& os,
                         SqliteResultCode sqlite_result_code) {
  return os << static_cast<int>(sqlite_result_code);
}

std::ostream& operator<<(std::ostream& os, SqliteErrorCode sqlite_error_code) {
  return os << static_cast<SqliteResultCode>(sqlite_error_code);
}

void CheckSqliteLoggedResultCodeForTesting() {
  // Ensure that error codes are alphabetical.
  const auto* unordered_it = base::ranges::adjacent_find(
      kResultCodeMapping,
      [](SqliteResultCodeMappingEntry lhs, SqliteResultCodeMappingEntry rhs) {
        return lhs.result_code >= rhs.result_code;
      });
  DCHECK_EQ(unordered_it, std::ranges::end(kResultCodeMapping))
      << "Mapping ordering broken at {" << unordered_it->result_code << ", "
      << static_cast<int>(unordered_it->logged_code) << "}";

  std::set<int> sqlite_result_codes;
  for (auto& mapping_entry : kResultCodeMapping) {
    sqlite_result_codes.insert(mapping_entry.result_code);
  }

  // SQLite doesn't have special messages for extended errors.
  // At the time of this writing, sqlite3_errstr() has a string table for
  // primary result codes, and uses it for extended error codes as well.
  //
  // So, we can only use sqlite3_errstr() to check for holes in the primary
  // message table.
  for (int result_code = 0; result_code <= 256; ++result_code) {
    if (sqlite_result_codes.count(result_code) != 0) {
      continue;
    }

    const char* error_message = sqlite3_errstr(result_code);

    static constexpr std::string_view kUnknownErrorMessage("unknown error");
    DCHECK_EQ(kUnknownErrorMessage.compare(error_message), 0)
        << "Unmapped SQLite result code: " << result_code
        << " SQLite message: " << error_message;
  }

  // Number of #defines in https://www.sqlite.org/c3ref/c_abort.html
  //
  // This number is also stated at
  // https://www.sqlite.org/rescode.html#primary_result_code_list
  static constexpr int kPrimaryResultCodes = 31;

  // Number of #defines in https://www.sqlite.org/c3ref/c_abort_rollback.html
  //
  // This number is also stated at
  // https://www.sqlite.org/rescode.html#extended_result_code_list
  static constexpr int kExtendedResultCodes = 74;

  DCHECK_EQ(std::size(kResultCodeMapping),
            size_t{kPrimaryResultCodes + kExtendedResultCodes})
      << "Mapping table has incorrect number of entries";
}

}  // namespace sql
