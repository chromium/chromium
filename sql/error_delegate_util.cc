// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/error_delegate_util.h"

#include <ostream>  // Needed to compile NOTREACHED() with operator <<.
#include <string>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

bool IsErrorCatastrophic(int sqlite_error_code) {
  // SQLite result codes are documented at https://www.sqlite.org/rescode.html
  int primary_error_code = sqlite_error_code & 0xff;

  // Within each group, error codes are sorted by their numerical values. This
  // matches the order used by the SQLite documentation describing them.
  switch (primary_error_code) {
    // Group of error codes that should never be returned by SQLite.
    //
    // If we do get these, our database schema / query pattern / data managed to
    // trigger a bug in SQLite. In development, we DCHECK to flag this SQLite
    // bug. In production, we [[fallback]] to corruption handling, because the
    // bug may be persistent, and corruption recovery will get the user unstuck.
    case SQLITE_INTERNAL:  // Bug in SQLite.
    case SQLITE_EMPTY:     // Marked for SQLite internal use.
    case SQLITE_FORMAT:    // Not currently used, according to SQLite docs.
    case SQLITE_NOTICE:    // Only used as an argument to sqlite3_log().
    case SQLITE_WARNING:   // Only used as an argument to sqlite3_log().
      NOTREACHED() << "SQLite returned result code marked for internal use: "
                   << sqlite_error_code;

    // Group of error codes that may only be returned by SQLite (given Chrome's
    // usage patterns) if a database is corrupted. DCHECK would not be
    // appropriate, since these can occur in production. Silently [[fallback]]
    // to corruption handling.
    case SQLITE_ERROR:
      // Generic/fallback error code.
      //
      // In production, database corruption leads our SQL statements being
      // flagged as invalid. For example, a SQL statement may reference a table
      // or column whose name got corrupted.
      //
      // In development, this error code shows up most often when passing
      // invalid SQL statements to SQLite. We have DCHECKs in sql::Statement and
      // sql::Database::Execute() that catch obvious SQL syntax errors. We can't
      // DCHECK when a SQL statement uses incorrect table/index/row names,
      // because that can legitimately happen in production, due to corruption.
      //
      // In 2022 we considered these errors as non-catastrophic, and we didn't
      // find ANY invalid SQL statements, and only found failed transactions
      // and schemas that didn't match the reported schema version, which both
      // suggest corruption. See https://crbug.com/1321483 for context.
      [[fallthrough]];
    case SQLITE_PERM:
      // Failed to get the requested access mode for a newly created database.
      // The database was just created, so error recovery will not cause data
      // loss. Error recovery steps, such as re-creating database files, may
      // fix the permission problems.
      [[fallthrough]];
    case SQLITE_CORRUPT:
      // Some form of database corruption was detected. The sql::Recovery code
      // may be able to recover some of the data.
      [[fallthrough]];
    case SQLITE_CANTOPEN:
      // Failed to open the database, for a variety of reasons. All the reasons
      // come down to some form of corruption. Here are some known reasons:
      // * One of the file names (database, journal, WAL, etc.) points to a
      //   directory, not a file. This indicates filesystem corruption. Most
      //   likely, some app messed with the user's Chrome file. It's also
      //   possible that the inode was corrupted and the is_dir bit flipped.
      // * One of the file names is a symlink, and SQLite was instructed not to
      //   follow symlinks. This should not occur in Chrome, we let SQLite use
      //   its default symlink handling.
      // * The WAL file has a format version that SQLite can't understand. This
      //   should not occur in Chrome, as we don't use WAL yet.
      [[fallthrough]];
    case SQLITE_MISMATCH:
      // SQLite was forced to perform an operation that involves incompatible
      // data types. An example is attempting to store a non-integer value in a
      // ROWID primary key.
      //
      // In production, database corruption can lead to this. For example, it's
      // possible that a schema is corrupted in such a way that the ROWID
      // primary key column's name is swapped with another column's name.
      [[fallthrough]];
    case SQLITE_NOLFS:
      // The database failed to grow past the filesystem size limit. This is
      // unlikely to happen in Chrome, but it is theoretically possible.
      [[fallthrough]];
    case SQLITE_NOTADB:
      // The database header is corrupted. The sql::Recovery code will not be
      // able to recovery any data, as SQLite will refuse to open the database.
      return true;

    // Group of result codes that are not error codes. These should never make
    // it to error handling code. In development, we DCHECK to flag this Chrome
    // bug. In production, we hope this is a transient error, such as a race
    // condition.
    case SQLITE_OK:    // Most used success code.
    case SQLITE_ROW:   // The statement produced a row of output.
    case SQLITE_DONE:  // A step has completed in a multi-step operation.
      NOTREACHED() << "Called with non-error result code " << sqlite_error_code;

    // Group of error codes that should not be returned by SQLite given Chrome's
    // usage patterns, even if the database gets corrupted. In development, we
    // DCHECK to flag this Chrome bug. In production, we hope the errors have
    // transient causes, such as race conditions.
    case SQLITE_LOCKED:
      // Conflict between two concurrently executing statements in the same
      // database connection.
      //
      // In theory, SQLITE_LOCKED could also signal a conflict between different
      // connections (in the same process) sharing a page cache, but Chrome only
      // uses private page caches.
      NOTREACHED() << "Conflict between concurrently executing SQL statements";
    case SQLITE_NOMEM:
      // Out of memory. This is most likely a transient error.
      //
      // There's a small chance that the error is caused by trying to exchange
      // too much data with SQLite. Most such errors result in SQLITE_TOOBIG.
      NOTREACHED() << "SQLite reported out-of-memory: " << sqlite_error_code;
    case SQLITE_INTERRUPT:
      // Chrome features don't use sqlite3_interrupt().
      NOTREACHED() << "SQLite returned INTERRUPT code: " << sqlite_error_code;
    case SQLITE_NOTFOUND:
      // Unknown opcode in sqlite3_file_control(). Chrome's features only use a
      // few built-in opcodes.
      NOTREACHED() << "SQLite returned NOTFOUND code: " << sqlite_error_code;
    case SQLITE_MISUSE:
      // SQLite API misuse, such as trying to use a prepared statement after it
      // was finalized. In development, we DCHECK to flag this Chrome bug. In
      // production, we hope this is a race condition, and therefore transient.
      NOTREACHED() << "SQLite returned MISUSE code: " << sqlite_error_code;
    case SQLITE_AUTH:
      // Chrome features don't install an authorizer callback. Only WebSQL does.
      NOTREACHED() << "SQLite returned AUTH code: " << sqlite_error_code;
    case SQLITE_RANGE:
      // Chrome uses DCHECKs to ensure the validity of column indexes passed to
      // sqlite3_bind() and sqlite3_column().
      NOTREACHED() << "SQLite returned RANGE code: " << sqlite_error_code;

    // Group of error codes that should may be returned by SQLite given Chrome's
    // usage patterns, even without database corruption. In development, we
    // DCHECK to flag this Chrome bug. In production, we hope the errors have
    // transient causes, such as race conditions.
    case SQLITE_ABORT:
      // SQLITE_ABORT may be returned when a ROLLBACK statement is executed
      // concurrently with a pending read or write, and Chrome features are
      // allowed to execute concurrent statements in the same transaction, under
      // some conditions.
      //
      // It may be worth noting that Chrome features don't use callback routines
      // that may abort SQL statements, such as passing a callback to
      // sqlite3_exec().
      [[fallthrough]];
    case SQLITE_BUSY:
      // Failed to grab a lock on the database. Another database connection
      // (most likely in another process) is holding the database lock. This
      // should not be a problem for exclusive databases, which are strongly
      // recommended for Chrome features.
      [[fallthrough]];
    case SQLITE_READONLY:
      // SQLite either failed to write to the database file or its associated
      // files (journal, WAL, etc.), or considers it unsafe to do so.
      //
      // Most error codes (SQLITE_READONLY_DIRECTORY, SQLITE_READONLY_RECOVERY,
      // SQLITE_READONLY_ROLLBACK, SQLITE_READONLY_CANTLOCK) mean that SQLite
      // failed to write to some file, or to create a file (which entails
      // writing to the directory containing the database).
      //
      // SQLITE_READONLY_CANTLOCK should never happen in Chrome, because we will
      // only allow enabling WAL on databases that use exclusive locking.
      //
      // Unlike all other codes, SQLITE_READONLY_DBMOVED signals that a file was
      // deleted or renamed. It is returned when SQLite realizes that the
      // database file was moved or unlinked from the filesystem after it was
      // opened, so the associated files (journal, WAL, etc.) would not be found
      // by another SQLite instance in the event of a crash. This was observed
      // on the iOS try bots.
      [[fallthrough]];
    case SQLITE_IOERR:
      // Catch-all for many errors reported by the VFS. Some of the errors
      // indicate media failure (SQLITE_IOERR_READ), while others indicate
      // transient problems (SQLITE_IOERR_LOCK). In the future, we may invest in
      // distinguishing between them. For now, since all the codes are bundled
      // up, we must assume that the error is transient.
      [[fallthrough]];
    case SQLITE_FULL:
      // The disk is full. This is definitely a transient error, and does not
      // indicate any database corruption. While it's true that the user will be
      // stuck in this state until some action is taken, we're unlikely to help
      // the user if we run our recovery code or delete our databases.
      [[fallthrough]];
    case SQLITE_PROTOCOL:
      // Gave up while attempting to grab a lock on a WAL database at the
      // beginning of a transaction. In theory, this should not be a problem in
      // Chrome, because we'll only allow enabling WAL on databases with
      // exclusive locking. However, other software on the user's system may
      // lock our databases in a way that triggers this error.
      [[fallthrough]];
    case SQLITE_SCHEMA:
      // The database schema was changed between the time when a prepared
      // statement was compiled, and when it was executing.
      //
      // This can happen in production. Databases that don't use exclusive
      // locking (recommended but not yet required for Chrome features) may be
      // changed from another process via legitimate use of SQLite APIs.
      // Databases that do use exclusive locks may still be mutated on-disk, on
      // operating systems where exclusive locks are only enforced via advisory
      // locking.
      //
      // When we mandate exclusive locks for all features in Chrome, we may
      // classify this error as database corruption, because it is an indicator
      // that another process is interfering with Chrome's schemas.
      [[fallthrough]];
    case SQLITE_TOOBIG:
      // SQLite encountered a string or blob whose length exceeds
      // SQLITE_MAX_LENGTH, or it was asked to execute a SQL statement whose
      // length exceeds SQLITE_MAX_SQL_LENGTH or SQLITE_LIMIT_SQL_LENGTH.
      //
      // A corrupted database could cause this in the following ways:
      // * SQLite could encounter an overly large string or blob because its
      //   size field got corrupted.
      // * SQLite could attempt to execute an overly large SQL statement while
      //   operating on a corrupted schema. (Some of SQLite's DDL statements
      //   involve executing SQL that includes schema content.)
      //
      // However, this could also occur due to a Chrome bug where we ask SQLite
      // to bind an overly large string or blob. So, we currently don't classify
      // this as definitely induced by corruption.
      [[fallthrough]];
    case SQLITE_CONSTRAINT:
      // This can happen in production, when executing SQL statements with the
      // semantics of "create a record if it doesn't exist, otherwise do
      // nothing".
      return false;
  }

  NOTREACHED() << "SQLite returned unknown result code: " << sqlite_error_code;
}

std::string GetCorruptFileDiagnosticsInfo(
    const base::FilePath& corrupted_file_path) {
  std::string corrupted_file_info("Corrupted file: ");
  corrupted_file_info +=
      corrupted_file_path.DirName().BaseName().AsUTF8Unsafe() + "/" +
      corrupted_file_path.BaseName().AsUTF8Unsafe() + "\n";
  return corrupted_file_info;
}

}  // namespace sql
