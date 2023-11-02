// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_SQLITE_RESULT_CODE_VALUES_H_
#define SQL_SQLITE_RESULT_CODE_VALUES_H_

namespace sql {

enum class SqliteResultCode : int {
  // See sqlite_result_code.h for a description of SqliteResultCode and
  // functions that create and consume it.
  //
  // The meaning of the codes is listed at https://www.sqlite.org/rescode.html
  //
  // Numerical enums are used here directly to avoid exposing sqlite3.h.
  // The .cc file has static_asserts ensuring the the enums match.
  // Lines are ordered by the numerical constant values.

  kOk = 0,                              // SQLITE_OK
  kError = 1,                           // SQLITE_ERROR
  kInternal = 2,                        // SQLITE_INTERNAL
  kPermission = 3,                      // SQLITE_PERM
  kAbort = 4,                           // SQLITE_ABORT
  kBusy = 5,                            // SQLITE_BUSY
  kLocked = 6,                          // SQLITE_LOCKED
  kNoMemory = 7,                        // SQLITE_NOMEM
  kReadOnly = 8,                        // SQLITE_READONLY
  kInterrupt = 9,                       // SQLITE_INTERRUPT
  kIo = 10,                             // SQLITE_IOERR
  kCorrupt = 11,                        // SQLITE_CORRUPT
  kNotFound = 12,                       // SQLITE_NOTFOUND
  kFullDisk = 13,                       // SQLITE_FULL
  kCantOpen = 14,                       // SQLITE_CANTOPEN
  kLockingProtocol = 15,                // SQLITE_PROTOCOL
  kEmpty = 16,                          // SQLITE_EMPTY
  kSchemaChanged = 17,                  // SQLITE_SCHEMA
  kTooBig = 18,                         // SQLITE_TOOBIG
  kConstraint = 19,                     // SQLITE_CONSTRAINT
  kTypeMismatch = 20,                   // SQLITE_MISMATCH
  kApiMisuse = 21,                      // SQLITE_MISUSE
  kNoLargeFileSupport = 22,             // SQLITE_NOLFS
  kUnauthorized = 23,                   // SQLITE_AUTH
  kFormat = 24,                         // SQLITE_FORMAT
  kIndexRange = 25,                     // SQLITE_RANGE
  kNotADatabase = 26,                   // SQLITE_NOTADB
  kLoggingNotice = 27,                  // SQLITE_NOTICE
  kLoggingWarning = 28,                 // SQLITE_WARNING
  kRow = 100,                           // SQLITE_ROW
  kDone = 101,                          // SQLITE_DONE
  kLoadPermanently = 256,               // SQLITE_OK_LOAD_PERMANENTLY
  kMissingCollatingSequence = 257,      // SQLITE_ERROR_MISSING_COLLSEQ
  kBusyRecovery = 261,                  // SQLITE_BUSY_RECOVERY
  kLockedSharedCache = 262,             // SQLITE_LOCKED_SHAREDCACHE
  kReadOnlyRecovery = 264,              // SQLITE_READONLY_RECOVERY
  kIoRead = 266,                        // SQLITE_IOERR_READ
  kCorruptVirtualTable = 267,           // SQLITE_CORRUPT_VTAB
  kCantOpenNoTemporaryDirectory = 270,  // SQLITE_CANTOPEN_NOTEMPDIR
  kConstraintCheck = 275,               // SQLITE_CONSTRAINT_CHECK
  kUnauthorizedUser = 279,              // SQLITE_AUTH_USER
  kLoggingNoticeRecoverWal = 283,       // SQLITE_NOTICE_RECOVER_WAL
  kLoggingWarningAutoIndex = 284,       // SQLITE_WARNING_AUTOINDEX
  kRetryPreparedStatement = 513,        // SQLITE_ERROR_RETRY
  kAbortRollback = 516,                 // SQLITE_ABORT_ROLLBACK
  kBusySnapshot = 517,                  // SQLITE_BUSY_SNAPSHOT
  kLockedVirtualTable = 518,            // SQLITE_LOCKED_VTAB
  kReadOnlyCantLock = 520,              // SQLITE_READONLY_CANTLOCK
  kIoShortRead = 522,                   // SQLITE_IOERR_SHORT_READ
  kCorruptSequence = 523,               // SQLITE_CORRUPT_SEQUENCE
  kCantOpenIsDir = 526,                 // SQLITE_CANTOPEN_ISDIR
  kConstraintCommitHook = 531,          // SQLITE_CONSTRAINT_COMMITHOOK
  kLoggingNoticeRecoverRollback = 539,  // SQLITE_NOTICE_RECOVER_ROLLBACK
  kErrorSnapshot = 769,                 // SQLITE_ERROR_SNAPSHOT
  kBusyTimeout = 773,                   // SQLITE_BUSY_TIMEOUT
  kReadOnlyRollback = 776,              // SQLITE_READONLY_ROLLBACK
  kIoWrite = 778,                       // SQLITE_IOERR_WRITE
  kCorruptIndex = 779,                  // SQLITE_CORRUPT_INDEX
  kCantOpenFullPath = 782,              // SQLITE_CANTOPEN_FULLPATH
  kConstraintForeignKey = 787,          // SQLITE_CONSTRAINT_FOREIGNKEY
  kReadOnlyDbMoved = 1032,              // SQLITE_READONLY_DBMOVED
  kIoFsync = 1034,                      // SQLITE_IOERR_FSYNC
  kCantOpenConvertPath = 1038,          // SQLITE_CANTOPEN_CONVPATH
  kConstraintFunction = 1043,           // SQLITE_CONSTRAINT_FUNCTION
  kReadOnlyCantInit = 1288,             // SQLITE_READONLY_CANTINIT
  kIoDirFsync = 1290,                   // SQLITE_IOERR_DIR_FSYNC
  kCantOpenDirtyWal = 1294,             // SQLITE_CANTOPEN_DIRTYWAL
  kConstraintNotNull = 1299,            // SQLITE_CONSTRAINT_NOTNULL
  kReadOnlyDirectory = 1544,            // SQLITE_READONLY_DIRECTORY
  kIoTruncate = 1546,                   // SQLITE_IOERR_TRUNCATE
  kCantOpenSymlink = 1550,              // SQLITE_CANTOPEN_SYMLINK
  kConstraintPrimaryKey = 1555,         // SQLITE_CONSTRAINT_PRIMARYKEY
  kIoFstat = 1802,                      // SQLITE_IOERR_FSTAT
  kConstraintTrigger = 1811,            // SQLITE_CONSTRAINT_TRIGGER
  kIoUnlock = 2058,                     // SQLITE_IOERR_UNLOCK
  kConstraintUnique = 2067,             // SQLITE_CONSTRAINT_UNIQUE
  kIoReadLock = 2314,                   // SQLITE_IOERR_RDLOCK
  kConstraintVirtualTable = 2323,       // SQLITE_CONSTRAINT_VTAB
  kIoDelete = 2570,                     // SQLITE_IOERR_DELETE
  kConstraintRowId = 2579,              // SQLITE_CONSTRAINT_ROWID
  kIoBlocked = 2826,                    // SQLITE_IOERR_BLOCKED
  kConstraintPinned = 2835,             // SQLITE_CONSTRAINT_PINNED
  kIoNoMemory = 3082,                   // SQLITE_IOERR_NOMEM
  kConstraintDataType = 3091,           // SQLITE_CONSTRAINT_DATATYPE
  kIoAccess = 3338,                     // SQLITE_IOERR_ACCESS
  kIoCheckReservedLock = 3594,          // SQLITE_IOERR_CHECKRESERVEDLOCK
  kIoLock = 3850,                       // SQLITE_IOERR_LOCK
  kIoClose = 4106,                      // SQLITE_IOERR_CLOSE
  kIoDirClose = 4362,                   // SQLITE_IOERR_DIR_CLOSE
  kIoSharedMemoryOpen = 4618,           // SQLITE_IOERR_SHMOPEN
  kIoSharedMemorySize = 4874,           // SQLITE_IOERR_SHMSIZE
  kIoSharedMemoryLock = 5130,           // SQLITE_IOERR_SHMLOCK
  kIoSharedMemoryMap = 5386,            // SQLITE_IOERR_SHMMAP
  kIoSeek = 5642,                       // SQLITE_IOERR_SEEK
  kIoDeleteNoEntry = 5898,              // SQLITE_IOERR_DELETE_NOENT
  kIoMemoryMapping = 6154,              // SQLITE_IOERR_MMAP
  kIoGetTemporaryPath = 6410,           // SQLITE_IOERR_GETTEMPPATH
  kIoConvertPath = 6666,                // SQLITE_IOERR_CONVPATH
  kIoVfsNode = 6922,                    // SQLITE_IOERR_VNODE
  kIoUnauthorized = 7178,               // SQLITE_IOERR_AUTH
  kIoBeginAtomic = 7434,                // SQLITE_IOERR_BEGIN_ATOMIC
  kIoCommitAtomic = 7690,               // SQLITE_IOERR_COMMIT_ATOMIC
  kIoRollbackAtomic = 7946,             // SQLITE_IOERR_ROLLBACK_ATOMIC
  kIoData = 8202,                       // SQLITE_IOERR_DATA
  kIoCorruptFileSystem = 8458,          // SQLITE_IOERR_CORRUPTFS
};

enum class SqliteErrorCode : int {
  // See sqlite_result_code.h for a description of SqliteErrorCode and functions
  // that create and consume it.
  //
  // The values here are a subset of SqliteResultCode values.
  // When adding new values, match the ordering in SqliteResultCode.

  kError = static_cast<int>(SqliteResultCode::kError),
  kInternal = static_cast<int>(SqliteResultCode::kInternal),
  kPermission = static_cast<int>(SqliteResultCode::kPermission),
  kAbort = static_cast<int>(SqliteResultCode::kAbort),
  kBusy = static_cast<int>(SqliteResultCode::kBusy),
  kLocked = static_cast<int>(SqliteResultCode::kLocked),
  kNoMemory = static_cast<int>(SqliteResultCode::kNoMemory),
  kReadOnly = static_cast<int>(SqliteResultCode::kReadOnly),
  kInterrupt = static_cast<int>(SqliteResultCode::kInterrupt),
  kIo = static_cast<int>(SqliteResultCode::kIo),
  kCorrupt = static_cast<int>(SqliteResultCode::kCorrupt),
  kNotFound = static_cast<int>(SqliteResultCode::kNotFound),
  kFullDisk = static_cast<int>(SqliteResultCode::kFullDisk),
  kCantOpen = static_cast<int>(SqliteResultCode::kCantOpen),
  kLockingProtocol = static_cast<int>(SqliteResultCode::kLockingProtocol),
  kEmpty = static_cast<int>(SqliteResultCode::kEmpty),
  kSchemaChanged = static_cast<int>(SqliteResultCode::kSchemaChanged),
  kTooBig = static_cast<int>(SqliteResultCode::kTooBig),
  kConstraint = static_cast<int>(SqliteResultCode::kConstraint),
  kTypeMismatch = static_cast<int>(SqliteResultCode::kTypeMismatch),
  kApiMisuse = static_cast<int>(SqliteResultCode::kApiMisuse),
  kNoLargeFileSupport = static_cast<int>(SqliteResultCode::kNoLargeFileSupport),
  kUnauthorized = static_cast<int>(SqliteResultCode::kUnauthorized),
  kFormat = static_cast<int>(SqliteResultCode::kFormat),
  kIndexRange = static_cast<int>(SqliteResultCode::kIndexRange),
  kNotADatabase = static_cast<int>(SqliteResultCode::kNotADatabase),
  kLoggingNotice = static_cast<int>(SqliteResultCode::kLoggingNotice),
  kLoggingWarning = static_cast<int>(SqliteResultCode::kLoggingWarning),
  kLoadPermanently = static_cast<int>(SqliteResultCode::kLoadPermanently),
  kMissingCollatingSequence =
      static_cast<int>(SqliteResultCode::kMissingCollatingSequence),
  kBusyRecovery = static_cast<int>(SqliteResultCode::kBusyRecovery),
  kLockedSharedCache = static_cast<int>(SqliteResultCode::kLockedSharedCache),
  kReadOnlyRecovery = static_cast<int>(SqliteResultCode::kReadOnlyRecovery),
  kIoRead = static_cast<int>(SqliteResultCode::kIoRead),
  kCorruptVirtualTable =
      static_cast<int>(SqliteResultCode::kCorruptVirtualTable),
  kCantOpenNoTemporaryDirectory =
      static_cast<int>(SqliteResultCode::kCantOpenNoTemporaryDirectory),
  kConstraintCheck = static_cast<int>(SqliteResultCode::kConstraintCheck),
  kUnauthorizedUser = static_cast<int>(SqliteResultCode::kUnauthorizedUser),
  kLoggingNoticeRecoverWal =
      static_cast<int>(SqliteResultCode::kLoggingNoticeRecoverWal),
  kLoggingWarningAutoIndex =
      static_cast<int>(SqliteResultCode::kLoggingWarningAutoIndex),
  kRetryPreparedStatement =
      static_cast<int>(SqliteResultCode::kRetryPreparedStatement),
  kAbortRollback = static_cast<int>(SqliteResultCode::kAbortRollback),
  kBusySnapshot = static_cast<int>(SqliteResultCode::kBusySnapshot),
  kLockedVirtualTable = static_cast<int>(SqliteResultCode::kLockedVirtualTable),
  kReadOnlyCantLock = static_cast<int>(SqliteResultCode::kReadOnlyCantLock),
  kIoShortRead = static_cast<int>(SqliteResultCode::kIoShortRead),
  kCorruptSequence = static_cast<int>(SqliteResultCode::kCorruptSequence),
  kCantOpenIsDir = static_cast<int>(SqliteResultCode::kCantOpenIsDir),
  kConstraintCommitHook =
      static_cast<int>(SqliteResultCode::kConstraintCommitHook),
  kLoggingNoticeRecoverRollback =
      static_cast<int>(SqliteResultCode::kLoggingNoticeRecoverRollback),
  kErrorSnapshot = static_cast<int>(SqliteResultCode::kErrorSnapshot),
  kBusyTimeout = static_cast<int>(SqliteResultCode::kBusyTimeout),
  kReadOnlyRollback = static_cast<int>(SqliteResultCode::kReadOnlyRollback),
  kIoWrite = static_cast<int>(SqliteResultCode::kIoWrite),
  kCorruptIndex = static_cast<int>(SqliteResultCode::kCorruptIndex),
  kCantOpenFullPath = static_cast<int>(SqliteResultCode::kCantOpenFullPath),
  kConstraintForeignKey =
      static_cast<int>(SqliteResultCode::kConstraintForeignKey),
  kReadOnlyDbMoved = static_cast<int>(SqliteResultCode::kReadOnlyDbMoved),
  kIoFsync = static_cast<int>(SqliteResultCode::kIoFsync),
  kCantOpenConvertPath =
      static_cast<int>(SqliteResultCode::kCantOpenConvertPath),
  kConstraintFunction = static_cast<int>(SqliteResultCode::kConstraintFunction),
  kReadOnlyCantInit = static_cast<int>(SqliteResultCode::kReadOnlyCantInit),
  kIoDirFsync = static_cast<int>(SqliteResultCode::kIoDirFsync),
  kCantOpenDirtyWal = static_cast<int>(SqliteResultCode::kCantOpenDirtyWal),
  kConstraintNotNull = static_cast<int>(SqliteResultCode::kConstraintNotNull),
  kReadOnlyDirectory = static_cast<int>(SqliteResultCode::kReadOnlyDirectory),
  kIoTruncate = static_cast<int>(SqliteResultCode::kIoTruncate),
  kCantOpenSymlink = static_cast<int>(SqliteResultCode::kCantOpenSymlink),
  kConstraintPrimaryKey =
      static_cast<int>(SqliteResultCode::kConstraintPrimaryKey),
  kIoFstat = static_cast<int>(SqliteResultCode::kIoFstat),
  kConstraintTrigger = static_cast<int>(SqliteResultCode::kConstraintTrigger),
  kIoUnlock = static_cast<int>(SqliteResultCode::kIoUnlock),
  kConstraintUnique = static_cast<int>(SqliteResultCode::kConstraintUnique),
  kIoReadLock = static_cast<int>(SqliteResultCode::kIoReadLock),
  kConstraintVirtualTable =
      static_cast<int>(SqliteResultCode::kConstraintVirtualTable),
  kIoDelete = static_cast<int>(SqliteResultCode::kIoDelete),
  kConstraintRowId = static_cast<int>(SqliteResultCode::kConstraintRowId),
  kIoBlocked = static_cast<int>(SqliteResultCode::kIoBlocked),
  kConstraintPinned = static_cast<int>(SqliteResultCode::kConstraintPinned),
  kIoNoMemory = static_cast<int>(SqliteResultCode::kIoNoMemory),
  kConstraintDataType = static_cast<int>(SqliteResultCode::kConstraintDataType),
  kIoAccess = static_cast<int>(SqliteResultCode::kIoAccess),
  kIoCheckReservedLock =
      static_cast<int>(SqliteResultCode::kIoCheckReservedLock),
  kIoLock = static_cast<int>(SqliteResultCode::kIoLock),
  kIoClose = static_cast<int>(SqliteResultCode::kIoClose),
  kIoDirClose = static_cast<int>(SqliteResultCode::kIoDirClose),
  kIoSharedMemoryOpen = static_cast<int>(SqliteResultCode::kIoSharedMemoryOpen),
  kIoSharedMemorySize = static_cast<int>(SqliteResultCode::kIoSharedMemorySize),
  kIoSharedMemoryLock = static_cast<int>(SqliteResultCode::kIoSharedMemoryLock),
  kIoSharedMemoryMap = static_cast<int>(SqliteResultCode::kIoSharedMemoryMap),
  kIoSeek = static_cast<int>(SqliteResultCode::kIoSeek),
  kIoDeleteNoEntry = static_cast<int>(SqliteResultCode::kIoDeleteNoEntry),
  kIoMemoryMapping = static_cast<int>(SqliteResultCode::kIoMemoryMapping),
  kIoGetTemporaryPath = static_cast<int>(SqliteResultCode::kIoGetTemporaryPath),
  kIoConvertPath = static_cast<int>(SqliteResultCode::kIoConvertPath),
  kIoVfsNode = static_cast<int>(SqliteResultCode::kIoVfsNode),
  kIoUnauthorized = static_cast<int>(SqliteResultCode::kIoUnauthorized),
  kIoBeginAtomic = static_cast<int>(SqliteResultCode::kIoBeginAtomic),
  kIoCommitAtomic = static_cast<int>(SqliteResultCode::kIoCommitAtomic),
  kIoRollbackAtomic = static_cast<int>(SqliteResultCode::kIoRollbackAtomic),
  kIoData = static_cast<int>(SqliteResultCode::kIoData),
  kIoCorruptFileSystem =
      static_cast<int>(SqliteResultCode::kIoCorruptFileSystem),
};

enum class SqliteLoggedResultCode : int {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // A success code: OK, DONE, ROW.
  kNoError = 0,

  // Codes that SQLite APIs should never return, such as SQLITE_INTERNAL.
  kUnusedSqlite = 1,

  // Codes that SQLite APIs should never return, given Chrome's usage pattern.
  kUnusedChrome = 2,

  // SQLITE_ERROR
  kGeneric = 3,

  // SQLITE_PERM
  kPermission = 4,

  // SQLITE_ABORT
  kAbort = 5,

  // SQLITE_BUSY
  kBusy = 6,

  // SQLITE_READONLY
  kReadOnly = 7,

  // SQLITE_IOERR
  kIo = 8,

  // SQLITE_CORRUPT
  kCorrupt = 9,

  // SQLITE_FULL
  kFullDisk = 10,

  // SQLITE_CANTOPEN
  kCantOpen = 11,

  // SQLITE_PROTOCOL
  kLockingProtocol = 12,

  // SQLITE_SCHEMA
  kSchemaChanged = 13,

  // SQLITE_TOOBIG
  kTooBig = 14,

  // SQLITE_CONSTRAINT
  kConstraint = 15,

  // SQLITE_MISMATCH
  kTypeMismatch = 16,

  // SQLITE_NOLFS
  kNoLargeFileSupport = 17,

  // SQLITE_NOTADB
  kNotADatabase = 18,

  // SQLITE_BUSY_RECOVERY
  kBusyRecovery = 19,

  // SQLITE_READONLY_RECOVERY
  kReadOnlyRecovery = 20,

  // SQLITE_IOERR_READ
  kIoRead = 21,

  // SQLITE_CONSTRAINT_CHECK
  kConstraintCheck = 22,

  // SQLITE_ABORT_ROLLBACK
  kAbortRollback = 23,

  // SQLITE_BUSY_SNAPSHOT
  kBusySnapshot = 24,

  // SQLITE_READONLY_CANTLOCK
  kReadOnlyCantLock = 25,

  // SQLITE_IOERR_SHORT_READ
  kIoShortRead = 26,

  // SQLITE_CORRUPT_SEQUENCE
  kCorruptSequence = 27,

  // SQLITE_CANTOPEN_ISDIR
  kCantOpenIsDir = 28,

  // SQLITE_READONLY_ROLLBACK
  kReadOnlyRollback = 29,

  // SQLITE_IOERR_WRITE
  kIoWrite = 30,

  // SQLITE_CORRUPT_INDEX
  kCorruptIndex = 31,

  // SQLITE_CONSTRAINT_FOREIGN_KEY
  kConstraintForeignKey = 32,

  // SQLITE_READONLY_DBMOVED
  kReadOnlyDbMoved = 33,

  // SQLITE_IOERR_FSYNC
  kIoFsync = 34,

  // SQLITE_IOERR_DIR_FSYNC
  kIoDirFsync = 35,

  // SQLITE_CONSTRAINT_NOTNULL
  kConstraintNotNull = 36,

  // SQLITE_READONLY_DIRECTORY
  kReadOnlyDirectory = 37,

  // SQLITE_IOERR_TRUNCATE
  kIoTruncate = 38,

  // SQLITE_CONSTRAINT_PRIMARYKEY
  kConstraintPrimaryKey = 39,

  // SQLITE_IOERR_FSTAT
  kIoFstat = 40,

  // SQLITE_IOERR_UNLOCK
  kIoUnlock = 41,

  // SQLITE_CONSTRAINT_UNIQUE
  kConstraintUnique = 42,

  // SQLITE_IOERR_RDLOCK
  kIoReadLock = 43,

  // SQLITE_IOERR_DELETE
  kIoDelete = 44,

  // SQLITE_CONSTRAINT_ROWID
  kConstraintRowId = 45,

  // SQLITE_CONSTRAINT_DATATYPE
  kConstraintDataType = 46,

  // SQLITE_IOERR_ACCESS
  kIoAccess = 47,

  // SQLITE_IOERR_CHECKRESERVEDLOCK
  kIoCheckReservedLock = 48,

  // SQLITE_IOERR_LOCK
  kIoLock = 49,

  // SQLITE_IOERR_CLOSE
  kIoClose = 50,

  // SQLITE_IOERR_SEEK
  kIoSeek = 51,

  // SQLITE_IOERR_DELETE_NOENT
  kIoDeleteNoEntry = 52,

  // SQLITE_IOERR_MMAP
  kIoMemoryMapping = 53,

  // SQLITE_IOERR_GETTEMPPATH
  kIoGetTemporaryPath = 54,

  // SQLITE_IOERR_BEGIN_ATOMIC
  kIoBeginAtomic = 55,

  // SQLITE_IOERR_COMMIT_ATOMIC
  kIoCommitAtomic = 56,

  // SQLITE_IOERR_ROLLBACK_ATOMIC
  kIoRollbackAtomic = 57,

  // SQLITE_IOERR_CORRUPTFS
  kIoCorruptFileSystem = 58,

  kMaxValue = kIoCorruptFileSystem,
};

}  // namespace sql

#endif  // SQL_SQLITE_RESULT_CODE_VALUES_H_
