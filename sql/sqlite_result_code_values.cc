// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/sqlite_result_code_values.h"

#include "third_party/sqlite/sqlite3.h"

namespace sql {

// This block ensures that the numerical values in the header match the
// constants exported by SQLite's header.

static_assert(static_cast<int>(SqliteResultCode::kOk) == SQLITE_OK);
static_assert(static_cast<int>(SqliteResultCode::kError) == SQLITE_ERROR);
static_assert(static_cast<int>(SqliteResultCode::kInternal) == SQLITE_INTERNAL);
static_assert(static_cast<int>(SqliteResultCode::kPermission) == SQLITE_PERM);
static_assert(static_cast<int>(SqliteResultCode::kAbort) == SQLITE_ABORT);
static_assert(static_cast<int>(SqliteResultCode::kBusy) == SQLITE_BUSY);
static_assert(static_cast<int>(SqliteResultCode::kLocked) == SQLITE_LOCKED);
static_assert(static_cast<int>(SqliteResultCode::kNoMemory) == SQLITE_NOMEM);
static_assert(static_cast<int>(SqliteResultCode::kReadOnly) == SQLITE_READONLY);
static_assert(static_cast<int>(SqliteResultCode::kInterrupt) ==
              SQLITE_INTERRUPT);
static_assert(static_cast<int>(SqliteResultCode::kIo) == SQLITE_IOERR);
static_assert(static_cast<int>(SqliteResultCode::kCorrupt) == SQLITE_CORRUPT);
static_assert(static_cast<int>(SqliteResultCode::kNotFound) == SQLITE_NOTFOUND);
static_assert(static_cast<int>(SqliteResultCode::kFullDisk) == SQLITE_FULL);
static_assert(static_cast<int>(SqliteResultCode::kCantOpen) == SQLITE_CANTOPEN);
static_assert(static_cast<int>(SqliteResultCode::kLockingProtocol) ==
              SQLITE_PROTOCOL);
static_assert(static_cast<int>(SqliteResultCode::kEmpty) == SQLITE_EMPTY);
static_assert(static_cast<int>(SqliteResultCode::kSchemaChanged) ==
              SQLITE_SCHEMA);
static_assert(static_cast<int>(SqliteResultCode::kTooBig) == SQLITE_TOOBIG);
static_assert(static_cast<int>(SqliteResultCode::kConstraint) ==
              SQLITE_CONSTRAINT);
static_assert(static_cast<int>(SqliteResultCode::kTypeMismatch) ==
              SQLITE_MISMATCH);
static_assert(static_cast<int>(SqliteResultCode::kApiMisuse) == SQLITE_MISUSE);
static_assert(static_cast<int>(SqliteResultCode::kNoLargeFileSupport) ==
              SQLITE_NOLFS);
static_assert(static_cast<int>(SqliteResultCode::kUnauthorized) == SQLITE_AUTH);
static_assert(static_cast<int>(SqliteResultCode::kFormat) == SQLITE_FORMAT);
static_assert(static_cast<int>(SqliteResultCode::kIndexRange) == SQLITE_RANGE);
static_assert(static_cast<int>(SqliteResultCode::kNotADatabase) ==
              SQLITE_NOTADB);
static_assert(static_cast<int>(SqliteResultCode::kLoggingNotice) ==
              SQLITE_NOTICE);
static_assert(static_cast<int>(SqliteResultCode::kLoggingWarning) ==
              SQLITE_WARNING);
static_assert(static_cast<int>(SqliteResultCode::kRow) == SQLITE_ROW);
static_assert(static_cast<int>(SqliteResultCode::kDone) == SQLITE_DONE);
static_assert(static_cast<int>(SqliteResultCode::kLoadPermanently) ==
              SQLITE_OK_LOAD_PERMANENTLY);
static_assert(static_cast<int>(SqliteResultCode::kMissingCollatingSequence) ==
              SQLITE_ERROR_MISSING_COLLSEQ);
static_assert(static_cast<int>(SqliteResultCode::kBusyRecovery) ==
              SQLITE_BUSY_RECOVERY);
static_assert(static_cast<int>(SqliteResultCode::kLockedSharedCache) ==
              SQLITE_LOCKED_SHAREDCACHE);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyRecovery) ==
              SQLITE_READONLY_RECOVERY);
static_assert(static_cast<int>(SqliteResultCode::kIoRead) == SQLITE_IOERR_READ);
static_assert(static_cast<int>(SqliteResultCode::kCorruptVirtualTable) ==
              SQLITE_CORRUPT_VTAB);
static_assert(
    static_cast<int>(SqliteResultCode::kCantOpenNoTemporaryDirectory) ==
    SQLITE_CANTOPEN_NOTEMPDIR);
static_assert(static_cast<int>(SqliteResultCode::kConstraintCheck) ==
              SQLITE_CONSTRAINT_CHECK);
static_assert(static_cast<int>(SqliteResultCode::kUnauthorizedUser) ==
              SQLITE_AUTH_USER);
static_assert(static_cast<int>(SqliteResultCode::kLoggingNoticeRecoverWal) ==
              SQLITE_NOTICE_RECOVER_WAL);
static_assert(static_cast<int>(SqliteResultCode::kLoggingWarningAutoIndex) ==
              SQLITE_WARNING_AUTOINDEX);
static_assert(static_cast<int>(SqliteResultCode::kRetryPreparedStatement) ==
              SQLITE_ERROR_RETRY);
static_assert(static_cast<int>(SqliteResultCode::kAbortRollback) ==
              SQLITE_ABORT_ROLLBACK);
static_assert(static_cast<int>(SqliteResultCode::kBusySnapshot) ==
              SQLITE_BUSY_SNAPSHOT);
static_assert(static_cast<int>(SqliteResultCode::kLockedVirtualTable) ==
              SQLITE_LOCKED_VTAB);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyCantLock) ==
              SQLITE_READONLY_CANTLOCK);
static_assert(static_cast<int>(SqliteResultCode::kIoShortRead) ==
              SQLITE_IOERR_SHORT_READ);
static_assert(static_cast<int>(SqliteResultCode::kCorruptSequence) ==
              SQLITE_CORRUPT_SEQUENCE);
static_assert(static_cast<int>(SqliteResultCode::kCantOpenIsDir) ==
              SQLITE_CANTOPEN_ISDIR);
static_assert(static_cast<int>(SqliteResultCode::kConstraintCommitHook) ==
              SQLITE_CONSTRAINT_COMMITHOOK);
static_assert(
    static_cast<int>(SqliteResultCode::kLoggingNoticeRecoverRollback) ==
    SQLITE_NOTICE_RECOVER_ROLLBACK);
static_assert(static_cast<int>(SqliteResultCode::kErrorSnapshot) ==
              SQLITE_ERROR_SNAPSHOT);
static_assert(static_cast<int>(SqliteResultCode::kBusyTimeout) ==
              SQLITE_BUSY_TIMEOUT);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyRollback) ==
              SQLITE_READONLY_ROLLBACK);
static_assert(static_cast<int>(SqliteResultCode::kIoWrite) ==
              SQLITE_IOERR_WRITE);
static_assert(static_cast<int>(SqliteResultCode::kCorruptIndex) ==
              SQLITE_CORRUPT_INDEX);
static_assert(static_cast<int>(SqliteResultCode::kCantOpenFullPath) ==
              SQLITE_CANTOPEN_FULLPATH);
static_assert(static_cast<int>(SqliteResultCode::kConstraintForeignKey) ==
              SQLITE_CONSTRAINT_FOREIGNKEY);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyDbMoved) ==
              SQLITE_READONLY_DBMOVED);
static_assert(static_cast<int>(SqliteResultCode::kIoFsync) ==
              SQLITE_IOERR_FSYNC);
static_assert(static_cast<int>(SqliteResultCode::kCantOpenConvertPath) ==
              SQLITE_CANTOPEN_CONVPATH);
static_assert(static_cast<int>(SqliteResultCode::kConstraintFunction) ==
              SQLITE_CONSTRAINT_FUNCTION);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyCantInit) ==
              SQLITE_READONLY_CANTINIT);
static_assert(static_cast<int>(SqliteResultCode::kIoDirFsync) ==
              SQLITE_IOERR_DIR_FSYNC);
static_assert(static_cast<int>(SqliteResultCode::kCantOpenDirtyWal) ==
              SQLITE_CANTOPEN_DIRTYWAL);
static_assert(static_cast<int>(SqliteResultCode::kConstraintNotNull) ==
              SQLITE_CONSTRAINT_NOTNULL);
static_assert(static_cast<int>(SqliteResultCode::kReadOnlyDirectory) ==
              SQLITE_READONLY_DIRECTORY);
static_assert(static_cast<int>(SqliteResultCode::kIoTruncate) ==
              SQLITE_IOERR_TRUNCATE);
static_assert(static_cast<int>(SqliteResultCode::kCantOpenSymlink) ==
              SQLITE_CANTOPEN_SYMLINK);
static_assert(static_cast<int>(SqliteResultCode::kConstraintPrimaryKey) ==
              SQLITE_CONSTRAINT_PRIMARYKEY);
static_assert(static_cast<int>(SqliteResultCode::kIoFstat) ==
              SQLITE_IOERR_FSTAT);
static_assert(static_cast<int>(SqliteResultCode::kConstraintTrigger) ==
              SQLITE_CONSTRAINT_TRIGGER);
static_assert(static_cast<int>(SqliteResultCode::kIoUnlock) ==
              SQLITE_IOERR_UNLOCK);
static_assert(static_cast<int>(SqliteResultCode::kConstraintUnique) ==
              SQLITE_CONSTRAINT_UNIQUE);
static_assert(static_cast<int>(SqliteResultCode::kIoReadLock) ==
              SQLITE_IOERR_RDLOCK);
static_assert(static_cast<int>(SqliteResultCode::kConstraintVirtualTable) ==
              SQLITE_CONSTRAINT_VTAB);
static_assert(static_cast<int>(SqliteResultCode::kIoDelete) ==
              SQLITE_IOERR_DELETE);
static_assert(static_cast<int>(SqliteResultCode::kConstraintRowId) ==
              SQLITE_CONSTRAINT_ROWID);
static_assert(static_cast<int>(SqliteResultCode::kIoBlocked) ==
              SQLITE_IOERR_BLOCKED);
static_assert(static_cast<int>(SqliteResultCode::kConstraintPinned) ==
              SQLITE_CONSTRAINT_PINNED);
static_assert(static_cast<int>(SqliteResultCode::kIoNoMemory) ==
              SQLITE_IOERR_NOMEM);
static_assert(static_cast<int>(SqliteResultCode::kConstraintDataType) ==
              SQLITE_CONSTRAINT_DATATYPE);
static_assert(static_cast<int>(SqliteResultCode::kIoAccess) ==
              SQLITE_IOERR_ACCESS);
static_assert(static_cast<int>(SqliteResultCode::kIoCheckReservedLock) ==
              SQLITE_IOERR_CHECKRESERVEDLOCK);
static_assert(static_cast<int>(SqliteResultCode::kIoLock) == SQLITE_IOERR_LOCK);
static_assert(static_cast<int>(SqliteResultCode::kIoClose) ==
              SQLITE_IOERR_CLOSE);
static_assert(static_cast<int>(SqliteResultCode::kIoDirClose) ==
              SQLITE_IOERR_DIR_CLOSE);
static_assert(static_cast<int>(SqliteResultCode::kIoSharedMemoryOpen) ==
              SQLITE_IOERR_SHMOPEN);
static_assert(static_cast<int>(SqliteResultCode::kIoSharedMemorySize) ==
              SQLITE_IOERR_SHMSIZE);
static_assert(static_cast<int>(SqliteResultCode::kIoSharedMemoryLock) ==
              SQLITE_IOERR_SHMLOCK);
static_assert(static_cast<int>(SqliteResultCode::kIoSharedMemoryMap) ==
              SQLITE_IOERR_SHMMAP);
static_assert(static_cast<int>(SqliteResultCode::kIoSeek) == SQLITE_IOERR_SEEK);
static_assert(static_cast<int>(SqliteResultCode::kIoDeleteNoEntry) ==
              SQLITE_IOERR_DELETE_NOENT);
static_assert(static_cast<int>(SqliteResultCode::kIoMemoryMapping) ==
              SQLITE_IOERR_MMAP);
static_assert(static_cast<int>(SqliteResultCode::kIoGetTemporaryPath) ==
              SQLITE_IOERR_GETTEMPPATH);
static_assert(static_cast<int>(SqliteResultCode::kIoConvertPath) ==
              SQLITE_IOERR_CONVPATH);
static_assert(static_cast<int>(SqliteResultCode::kIoVfsNode) ==
              SQLITE_IOERR_VNODE);
static_assert(static_cast<int>(SqliteResultCode::kIoUnauthorized) ==
              SQLITE_IOERR_AUTH);
static_assert(static_cast<int>(SqliteResultCode::kIoBeginAtomic) ==
              SQLITE_IOERR_BEGIN_ATOMIC);
static_assert(static_cast<int>(SqliteResultCode::kIoCommitAtomic) ==
              SQLITE_IOERR_COMMIT_ATOMIC);
static_assert(static_cast<int>(SqliteResultCode::kIoRollbackAtomic) ==
              SQLITE_IOERR_ROLLBACK_ATOMIC);
static_assert(static_cast<int>(SqliteResultCode::kIoData) == SQLITE_IOERR_DATA);
static_assert(static_cast<int>(SqliteResultCode::kIoCorruptFileSystem) ==
              SQLITE_IOERR_CORRUPTFS);

}  // namespace sql
