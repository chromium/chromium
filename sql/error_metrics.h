// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_ERROR_METRICS_H_
#define SQL_ERROR_METRICS_H_

#include "base/component_export.h"

namespace sql {

// Helper for logging a SQLite result code to a UMA histogram.
//
// The histogram should be declared as enum="SqliteLoggedResultCode".
//
// Works for all result codes, including success codes and extended error codes.
// DCHECKs if provided result code should not occur in Chrome's usage of SQLite.
COMPONENT_EXPORT(SQL)
void UmaHistogramSqliteResult(const char* histogram_name,
                              int sqlite_result_code);

// SQLite result codes, mapped into a more compact form for UMA logging.
//
// SQLite's (extended) result codes cover a wide range of integer values, and
// are not suitable for direct use with our UMA logging infrastructure. This
// enum compresses the range by removing gaps and by mapping multiple SQLite
// result codes to the same value where appropriate.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SqliteLoggedResultCode {
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

// Converts a SQLite result code into a UMA logging-friendly form.
//
// Works for all result codes, including success codes and extended error codes.
// DCHECKs if provided result code should not occur in Chrome's usage of SQLite.
//
// UmaHistogramSqliteResult() should be preferred for logging results to UMA.
COMPONENT_EXPORT(SQL)
SqliteLoggedResultCode CreateSqliteLoggedResultCode(int sqlite_result_code);

// Called by unit tests.
//
// DCHECKs the representation invariants of the mapping table used to convert
// SQLite result codes to logging-friendly values.
COMPONENT_EXPORT(SQL) void CheckSqliteLoggedResultCodeForTesting();

}  // namespace sql

#endif  // SQL_ERROR_METRICS_H_
