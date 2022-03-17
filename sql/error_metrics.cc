// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/error_metrics.h"

#include <ostream>  // Needed to compile NOTREACHED() with operator <<.
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

constexpr std::pair<int, SqliteLoggedResultCode> kResultCodeMapping[] = {
    // Entries are ordered by SQLite result code value. This should match the
    // ordering in https://www.sqlite.org/rescode.html.

    {SQLITE_OK, SqliteLoggedResultCode::kNoError},
    {SQLITE_ERROR, SqliteLoggedResultCode::kGeneric},
    {SQLITE_INTERNAL, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_PERM, SqliteLoggedResultCode::kPermission},
    {SQLITE_ABORT, SqliteLoggedResultCode::kAbort},

    // Chrome features shouldn't execute conflicting statements concurrently.
    {SQLITE_LOCKED, SqliteLoggedResultCode::kUnusedChrome},

    // Chrome should crash on OOM.
    {SQLITE_NOMEM, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_READONLY, SqliteLoggedResultCode::kReadOnly},

    // Chrome doesn't use sqlite3_interrupt().
    {SQLITE_INTERRUPT, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR, SqliteLoggedResultCode::kIo},
    {SQLITE_CORRUPT, SqliteLoggedResultCode::kCorrupt},

    // Chrome only passes a few known-good opcodes to sqlite3_file_control().
    {SQLITE_NOTFOUND, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_FULL, SqliteLoggedResultCode::kFullDisk},
    {SQLITE_CANTOPEN, SqliteLoggedResultCode::kCantOpen},
    {SQLITE_PROTOCOL, SqliteLoggedResultCode::kLockingProtocol},
    {SQLITE_EMPTY, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_SCHEMA, SqliteLoggedResultCode::kSchemaChanged},
    {SQLITE_TOOBIG, SqliteLoggedResultCode::kTooBig},
    {SQLITE_CONSTRAINT, SqliteLoggedResultCode::kConstraint},
    {SQLITE_MISMATCH, SqliteLoggedResultCode::kTypeMismatch},

    // Chrome should not misuse SQLite's API.
    {SQLITE_MISUSE, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_NOLFS, SqliteLoggedResultCode::kNoLargeFileSupport},

    // Chrome does not set an authorizer callback.
    {SQLITE_AUTH, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_FORMAT, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome should not use invalid column indexes in sqlite3_{bind,column}*().
    {SQLITE_RANGE, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_NOTADB, SqliteLoggedResultCode::kNotADatabase},
    {SQLITE_NOTICE, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_WARNING, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_ROW, SqliteLoggedResultCode::kNoError},
    {SQLITE_DONE, SqliteLoggedResultCode::kNoError},
    {SQLITE_OK_LOAD_PERMANENTLY, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome should not use collating sequence names in SQL statements.
    {SQLITE_ERROR_MISSING_COLLSEQ, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_BUSY_RECOVERY, SqliteLoggedResultCode::kBusyRecovery},

    // Chrome does not use a shared page cache.
    {SQLITE_LOCKED_SHAREDCACHE, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_READONLY_RECOVERY, SqliteLoggedResultCode::kReadOnlyRecovery},
    {SQLITE_IOERR_READ, SqliteLoggedResultCode::kIoRead},

    // Chrome does not use a virtual table that signals corruption. We only use
    // a
    // virtual table code for recovery. That code does not use this error.
    {SQLITE_CORRUPT_VTAB, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_CANTOPEN_NOTEMPDIR, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_CONSTRAINT_CHECK, SqliteLoggedResultCode::kConstraintCheck},

    // Chrome does not set an authorizer callback.
    {SQLITE_AUTH_USER, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_NOTICE_RECOVER_WAL, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_WARNING_AUTOINDEX, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_ERROR_RETRY, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_ABORT_ROLLBACK, SqliteLoggedResultCode::kAbortRollback},
    {SQLITE_BUSY_SNAPSHOT, SqliteLoggedResultCode::kBusySnapshot},

    // Chrome does not use a virtual table that signals conflicts. We only use a
    // virtual table code for recovery. That code does not use this error.
    {SQLITE_LOCKED_VTAB, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_READONLY_CANTLOCK, SqliteLoggedResultCode::kReadOnlyCantLock},
    {SQLITE_IOERR_SHORT_READ, SqliteLoggedResultCode::kIoShortRead},
    {SQLITE_CORRUPT_SEQUENCE, SqliteLoggedResultCode::kCorruptSequence},
    {SQLITE_CANTOPEN_ISDIR, SqliteLoggedResultCode::kCantOpenIsDir},

    // Chrome does not use commit hook callbacks.
    {SQLITE_CONSTRAINT_COMMITHOOK, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_NOTICE_RECOVER_ROLLBACK, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome does not use sqlite3_snapshot_open().
    {SQLITE_ERROR_SNAPSHOT, SqliteLoggedResultCode::kUnusedChrome},
#ifdef SQLITE_ENABLE_SNAPSHOT
#error "This code assumes that Chrome does not use sqlite3_snapshot_open()"
#endif

    // Chrome does not use blocking Posix advisory file lock requests.
    {SQLITE_ERROR_SNAPSHOT, SqliteLoggedResultCode::kUnusedChrome},
#ifdef SQLITE_ENABLE_SETLK_TIMEOUT
#error "This code assumes that Chrome does not use
#endif

    {SQLITE_READONLY_ROLLBACK, SqliteLoggedResultCode::kReadOnlyRollback},
    {SQLITE_IOERR_WRITE, SqliteLoggedResultCode::kIoWrite},
    {SQLITE_CORRUPT_INDEX, SqliteLoggedResultCode::kCorruptIndex},

    // Chrome should always pass full paths to SQLite.
    {SQLITE_CANTOPEN_FULLPATH, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_CONSTRAINT_FOREIGNKEY,
     SqliteLoggedResultCode::kConstraintForeignKey},
    {SQLITE_READONLY_DBMOVED, SqliteLoggedResultCode::kReadOnlyDbMoved},
    {SQLITE_IOERR_FSYNC, SqliteLoggedResultCode::kIoFsync},

    // Chrome does not support Cygwin and does not use its VFS.
    {SQLITE_CANTOPEN_CONVPATH, SqliteLoggedResultCode::kUnusedChrome},

    // Chrome does not use extension functions.
    {SQLITE_CONSTRAINT_FUNCTION, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_READONLY_CANTINIT, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_IOERR_DIR_FSYNC, SqliteLoggedResultCode::kIoDirFsync},
    {SQLITE_CANTOPEN_DIRTYWAL, SqliteLoggedResultCode::kUnusedSqlite},
    {SQLITE_CONSTRAINT_NOTNULL, SqliteLoggedResultCode::kConstraintNotNull},
    {SQLITE_READONLY_DIRECTORY, SqliteLoggedResultCode::kReadOnlyDirectory},
    {SQLITE_IOERR_TRUNCATE, SqliteLoggedResultCode::kIoTruncate},

    // Chrome does not use the SQLITE_OPEN_NOFOLLOW flag.
    {SQLITE_CANTOPEN_SYMLINK, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_CONSTRAINT_PRIMARYKEY,
     SqliteLoggedResultCode::kConstraintPrimaryKey},
    {SQLITE_IOERR_FSTAT, SqliteLoggedResultCode::kIoFstat},

    // Chrome unconditionally disables database triggers via
    // sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_TRIGGER).
    {SQLITE_CONSTRAINT_TRIGGER, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_UNLOCK, SqliteLoggedResultCode::kIoUnlock},
    {SQLITE_CONSTRAINT_UNIQUE, SqliteLoggedResultCode::kConstraintUnique},
    {SQLITE_IOERR_RDLOCK, SqliteLoggedResultCode::kIoReadLock},

    // Chrome does not use a virtual table that signals constraints. We only use
    // a virtual table code for recovery. That code does not use this error.
    {SQLITE_CONSTRAINT_VTAB, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_DELETE, SqliteLoggedResultCode::kIoDelete},
    {SQLITE_CONSTRAINT_ROWID, SqliteLoggedResultCode::kConstraintRowId},
    {SQLITE_IOERR_BLOCKED, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome unconditionally disables database triggers via
    // sqlite3_db_config(SQLITE_DBCONFIG_ENABLE_TRIGGER).
    {SQLITE_CONSTRAINT_PINNED, SqliteLoggedResultCode::kUnusedChrome},

    // The SQLite docus claim that this error code is "normally" converted to
    // SQLITE_NOMEM. This doesn't seem 100% categorical, so we're flagging this
    // as "unused in Chrome" per the same rationale as SQLITE_NOMEM.
    {SQLITE_IOERR_NOMEM, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_CONSTRAINT_DATATYPE, SqliteLoggedResultCode::kConstraintDataType},
    {SQLITE_IOERR_ACCESS, SqliteLoggedResultCode::kIoAccess},
    {SQLITE_IOERR_CHECKRESERVEDLOCK,
     SqliteLoggedResultCode::kIoCheckReservedLock},
    {SQLITE_IOERR_LOCK, SqliteLoggedResultCode::kIoLock},
    {SQLITE_IOERR_CLOSE, SqliteLoggedResultCode::kIoClose},
    {SQLITE_IOERR_DIR_CLOSE, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMOPEN, SqliteLoggedResultCode::kUnusedChrome},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMSIZE, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_SHMLOCK, SqliteLoggedResultCode::kUnusedSqlite},

    // Chrome will only allow enabling WAL on databases with exclusive locking.
    {SQLITE_IOERR_SHMMAP, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_SEEK, SqliteLoggedResultCode::kIoSeek},
    {SQLITE_IOERR_DELETE_NOENT, SqliteLoggedResultCode::kIoDeleteNoEntry},
    {SQLITE_IOERR_MMAP, SqliteLoggedResultCode::kIoMemoryMapping},
    {SQLITE_IOERR_GETTEMPPATH, SqliteLoggedResultCode::kIoGetTemporaryPath},

    // Chrome does not support Cygwin and does not use its VFS.
    {SQLITE_IOERR_CONVPATH, SqliteLoggedResultCode::kUnusedChrome},

    // Chrome does not use SQLite extensions.
    {SQLITE_IOERR_VNODE, SqliteLoggedResultCode::kUnusedChrome},

    // Chrome does not use SQLite extensions.
    {SQLITE_IOERR_AUTH, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_BEGIN_ATOMIC, SqliteLoggedResultCode::kIoBeginAtomic},
    {SQLITE_IOERR_COMMIT_ATOMIC, SqliteLoggedResultCode::kIoCommitAtomic},
    {SQLITE_IOERR_ROLLBACK_ATOMIC, SqliteLoggedResultCode::kIoRollbackAtomic},

    // Chrome does not use the checksum VFS shim.
    {SQLITE_IOERR_DATA, SqliteLoggedResultCode::kUnusedChrome},

    {SQLITE_IOERR_CORRUPTFS, SqliteLoggedResultCode::kIoCorruptFileSystem},
};

}  // namespace

SqliteLoggedResultCode CreateSqliteLoggedResultCode(int sqlite_result_code) {
  const auto* mapping_it = base::ranges::find_if(
      kResultCodeMapping,
      [&sqlite_result_code](const std::pair<int, SqliteLoggedResultCode>& rhs) {
        return sqlite_result_code == rhs.first;
      });

  if (mapping_it == base::ranges::end(kResultCodeMapping)) {
    NOTREACHED() << "Unsupported SQLite result code: " << sqlite_result_code;
    return SqliteLoggedResultCode::kUnusedChrome;
  }
  SqliteLoggedResultCode logged_code = mapping_it->second;

  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedSqlite)
      << "SQLite reported code marked for internal use: " << sqlite_result_code;
  DCHECK_NE(logged_code, SqliteLoggedResultCode::kUnusedChrome)
      << "SQLite reported code that should never show up in Chrome: "
      << sqlite_result_code;
  return logged_code;
}

void UmaHistogramSqliteResult(const char* histogram_name,
                              int sqlite_result_code) {
  auto logged_code = CreateSqliteLoggedResultCode(sqlite_result_code);
  base::UmaHistogramEnumeration(histogram_name, logged_code);
}

}  // namespace sql
