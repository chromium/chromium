// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recovery.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"
#include "sql/database.h"
#include "sql/recover_module/module.h"
#include "sql/statement.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

// static
std::unique_ptr<Recovery> Recovery::Begin(Database* database,
                                          const base::FilePath& db_path) {
  // Recovery is likely to be used in error handling.  Since recovery changes
  // the state of the handle, protect against multiple layers attempting the
  // same recovery.
  if (!database->is_open()) {
    // Warn about API mis-use.
    DCHECK(database->poisoned(InternalApiToken()))
        << "Illegal to recover with closed Database";
    return nullptr;
  }

  // Using `new` to access a non-public constructor
  std::unique_ptr<Recovery> recovery(new Recovery(database));
  if (!recovery->Init(db_path)) {
    // TODO(shess): Should Init() failure result in Raze()?
    recovery->Shutdown(POISON);
    return nullptr;
  }

  return recovery;
}

// static
bool Recovery::Recovered(std::unique_ptr<Recovery> r) {
  return r->Backup();
}

// static
void Recovery::Unrecoverable(std::unique_ptr<Recovery> r) {
  CHECK(r->db_);
  // ~Recovery() will RAZE_AND_POISON.
}

// static
void Recovery::Rollback(std::unique_ptr<Recovery> r) {
  // TODO(shess): Crash / crash and dump?
  r->Shutdown(POISON);
}

Recovery::Recovery(Database* connection)
    : db_(connection),
      recover_db_({
          .exclusive_locking = false,
          .page_size = db_->page_size(),
          // The interface to the recovery module is a virtual table.
          .enable_virtual_tables_discouraged = true,
      }) {
  // Files with I/O errors cannot be safely memory-mapped.
  recover_db_.set_mmap_disabled();

  // TODO(shess): This may not handle cases where the default page
  // size is used, but the default has changed.  I do not think this
  // has ever happened.  This could be handled by using "PRAGMA
  // page_size", at the cost of potential additional failure cases.
}

Recovery::~Recovery() {
  Shutdown(RAZE_AND_POISON);
}

bool Recovery::Init(const base::FilePath& db_path) {
#if DCHECK_IS_ON()
  // set_error_callback() will DCHECK if the database already has an error
  // callback. The recovery process is likely to result in SQLite errors, and
  // those shouldn't get surfaced to any callback.
  db_->set_error_callback(base::BindRepeating(
      [](int sqlite_error_code, sql::Statement* statement) {}));

  // Undo the set_error_callback() above. We only used it for its DCHECK
  // behavior.
  db_->reset_error_callback();
#endif  // DCHECK_IS_ON()

  // Break any outstanding transactions on the original database to
  // prevent deadlocks reading through the attached version.
  // TODO(shess): A client may legitimately wish to recover from
  // within the transaction context, because it would potentially
  // preserve any in-flight changes.  Unfortunately, any attach-based
  // system could not handle that.  A system which manually queried
  // one database and stored to the other possibly could, but would be
  // more complicated.
  db_->RollbackAllTransactions();

  // Disable exclusive locking mode so that the attached database can
  // access things.  The locking_mode change is not active until the
  // next database access, so immediately force an access.  Enabling
  // writable_schema allows processing through certain kinds of
  // corruption.
  // TODO(shess): It would be better to just close the handle, but it
  // is necessary for the final backup which rewrites things.  It
  // might be reasonable to close then re-open the handle.
  std::ignore = db_->Execute("PRAGMA writable_schema=1");
  std::ignore = db_->Execute("PRAGMA locking_mode=NORMAL");
  std::ignore = db_->Execute("SELECT COUNT(*) FROM sqlite_schema");

  // TODO(shess): If this is a common failure case, it might be
  // possible to fall back to a memory database.  But it probably
  // implies that the SQLite tmpdir logic is busted, which could cause
  // a variety of other random issues in our code.
  if (!recover_db_.OpenTemporary(base::PassKey<Recovery>()))
    return false;

  // Enable the recover virtual table for this connection.
  int rc = EnableRecoveryExtension(&recover_db_, InternalApiToken());
  if (rc != SQLITE_OK) {
    LOG(ERROR) << "Failed to initialize recover module: "
               << recover_db_.GetErrorMessage();
    return false;
  }

  // Turn on |SQLITE_RecoveryMode| for the handle, which allows
  // reading certain broken databases.
  if (!recover_db_.Execute("PRAGMA writable_schema=1"))
    return false;

  if (!recover_db_.AttachDatabase(db_path, "corrupt", InternalApiToken()))
    return false;

  return true;
}

bool Recovery::Backup() {
  CHECK(db_);
  CHECK(recover_db_.is_open());

  // TODO(shess): Some of the failure cases here may need further
  // exploration.  Just as elsewhere, persistent problems probably
  // need to be razed, while anything which might succeed on a future
  // run probably should be allowed to try.  But since Raze() uses the
  // same approach, even that wouldn't work when this code fails.
  //
  // The documentation for the backup system indicate a relatively
  // small number of errors are expected:
  // SQLITE_BUSY - cannot lock the destination database.  This should
  //               only happen if someone has another handle to the
  //               database, Chromium generally doesn't do that.
  // SQLITE_LOCKED - someone locked the source database.  Should be
  //                 impossible (perhaps anti-virus could?).
  // SQLITE_READONLY - destination is read-only.
  // SQLITE_IOERR - since source database is temporary, probably
  //                indicates that the destination contains blocks
  //                throwing errors, or gross filesystem errors.
  // SQLITE_NOMEM - out of memory, should be transient.
  //
  // AFAICT, SQLITE_BUSY and SQLITE_NOMEM could perhaps be considered
  // transient, with SQLITE_LOCKED being unclear.
  //
  // SQLITE_READONLY and SQLITE_IOERR are probably persistent, with a
  // strong chance that Raze() would not resolve them.  If Delete()
  // deletes the database file, the code could then re-open the file
  // and attempt the backup again.
  //
  // For now, this code attempts a best effort.

  // Backup the original db from the recovered db.
  const char* kMain = "main";
  sqlite3_backup* backup =
      sqlite3_backup_init(db_->db(InternalApiToken()), kMain,
                          recover_db_.db(InternalApiToken()), kMain);
  if (!backup) {
    // Error code is in the destination database handle.
    LOG(ERROR) << "sqlite3_backup_init() failed: "
               << sqlite3_errmsg(db_->db(InternalApiToken()));

    return false;
  }

  // -1 backs up the entire database.
  int rc = sqlite3_backup_step(backup, -1);
  int pages = sqlite3_backup_pagecount(backup);
  // TODO(shess): sqlite3_backup_finish() appears to allow returning a
  // different value from sqlite3_backup_step().  Circle back and
  // figure out if that can usefully inform the decision of whether to
  // retry or not.
  sqlite3_backup_finish(backup);
  DCHECK_GT(pages, 0);

  if (rc != SQLITE_DONE) {
    LOG(ERROR) << "sqlite3_backup_step() failed: "
               << sqlite3_errmsg(db_->db(InternalApiToken()));
  }

  // The destination database was locked.  Give up, but leave the data
  // in place.  Maybe it won't be locked next time.
  if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
    Shutdown(POISON);
    return false;
  }

  // Running out of memory should be transient, retry later.
  if (rc == SQLITE_NOMEM) {
    Shutdown(POISON);
    return false;
  }

  // TODO(shess): For now, leave the original database alone. Some errors should
  // probably route to RAZE_AND_POISON.
  if (rc != SQLITE_DONE) {
    Shutdown(POISON);
    return false;
  }

  // Clean up the recovery db, and terminate the main database
  // connection.
  Shutdown(POISON);
  return true;
}

void Recovery::Shutdown(Recovery::Disposition raze) {
  if (!db_)
    return;

  recover_db_.Close();
  if (raze == RAZE_AND_POISON) {
    db_->RazeAndPoison();
  } else if (raze == POISON) {
    db_->Poison();
  }
  db_ = nullptr;
}

bool Recovery::AutoRecoverTable(const char* table_name,
                                size_t* rows_recovered) {
  // Query the info for the recovered table in database [main].
  std::string query(
      base::StringPrintf("PRAGMA main.table_info(%s)", table_name));
  Statement s(db()->GetUniqueStatement(query.c_str()));

  // The columns of the recover virtual table.
  std::vector<std::string> create_column_decls;

  // The columns to select from the recover virtual table when copying
  // to the recovered table.
  std::vector<std::string> insert_columns;

  // If PRIMARY KEY is a single INTEGER column, then it is an alias
  // for ROWID.  The primary key can be compound, so this can only be
  // determined after processing all column data and tracking what is
  // seen.  |pk_column_count| counts the columns in the primary key.
  // |rowid_decl| stores the ROWID version of the last INTEGER column
  // seen, which is at |rowid_ofs| in |create_column_decls|.
  size_t pk_column_count = 0;
  size_t rowid_ofs = 0;  // Only valid if rowid_decl is set.
  std::string rowid_decl;  // ROWID version of column |rowid_ofs|.

  while (s.Step()) {
    const std::string column_name(s.ColumnString(1));
    const std::string column_type(s.ColumnString(2));
    const ColumnType default_type = s.GetColumnType(4);
    const bool default_is_null = (default_type == ColumnType::kNull);
    const int pk_column = s.ColumnInt(5);

    // http://www.sqlite.org/pragma.html#pragma_table_info documents column 5 as
    // the 1-based index of the column in the primary key, otherwise 0.
    if (pk_column > 0)
      ++pk_column_count;

    // Construct column declaration as "name type [optional constraint]".
    std::string column_decl = column_name;

    // SQLite's affinity detection is documented at:
    // http://www.sqlite.org/datatype3.html#affname
    // The gist of it is that CHAR, TEXT, and INT use substring matches.
    // TODO(shess): It would be nice to unit test the type handling,
    // but it is not obvious to me how to write a test which would
    // fail appropriately when something was broken.  It would have to
    // somehow use data which would allow detecting the various type
    // coercions which happen.  If STRICT could be enabled, type
    // mismatches could be detected by which rows are filtered.
    if (column_type.find("INT") != std::string::npos) {
      if (pk_column == 1) {
        rowid_ofs = create_column_decls.size();
        rowid_decl = column_name + " ROWID";
      }
      column_decl += " INTEGER";
    } else if (column_type.find("CHAR") != std::string::npos ||
               column_type.find("TEXT") != std::string::npos) {
      column_decl += " TEXT";
    } else if (column_type == "BLOB") {
      column_decl += " BLOB";
    } else if (column_type.find("DOUB") != std::string::npos) {
      column_decl += " FLOAT";
    } else {
      // TODO(shess): AFAICT, there remain:
      // - contains("CLOB") -> TEXT
      // - contains("REAL") -> FLOAT
      // - contains("FLOA") -> FLOAT
      // - other -> "NUMERIC"
      // Just code those in as they come up.
      NOTREACHED() << " Unsupported type " << column_type;
      return false;
    }

    create_column_decls.push_back(column_decl);

    // Per the NOTE in the header file, convert NULL values to the
    // DEFAULT.  All columns could be IFNULL(column_name,default), but
    // the NULL case would require special handling either way.
    if (default_is_null) {
      insert_columns.push_back(column_name);
    } else {
      // The default value appears to be pre-quoted, as if it is
      // literally from the sqlite_schema CREATE statement.
      std::string default_value = s.ColumnString(4);
      insert_columns.push_back(base::StringPrintf(
          "IFNULL(%s,%s)", column_name.c_str(), default_value.c_str()));
    }
  }

  // Receiving no column information implies that the table doesn't exist.
  if (create_column_decls.empty()) {
    return false;
  }

  // If the PRIMARY KEY was a single INTEGER column, convert it to ROWID.
  if (pk_column_count == 1 && !rowid_decl.empty())
    create_column_decls[rowid_ofs] = rowid_decl;

  std::string recover_create(base::StringPrintf(
      "CREATE VIRTUAL TABLE temp.recover_%s USING recover(corrupt.%s, %s)",
      table_name,
      table_name,
      base::JoinString(create_column_decls, ",").c_str()));

  // INSERT OR IGNORE means that it will drop rows resulting from constraint
  // violations.  INSERT OR REPLACE only handles UNIQUE constraint violations.
  std::string recover_insert(base::StringPrintf(
      "INSERT OR IGNORE INTO main.%s SELECT %s FROM temp.recover_%s",
      table_name,
      base::JoinString(insert_columns, ",").c_str(),
      table_name));

  std::string recover_drop(base::StringPrintf(
      "DROP TABLE temp.recover_%s", table_name));

  if (!db()->Execute(recover_create.c_str()))
    return false;

  if (!db()->Execute(recover_insert.c_str())) {
    std::ignore = db()->Execute(recover_drop.c_str());
    return false;
  }

  *rows_recovered = db()->GetLastChangeCount();

  // TODO(shess): Is leaving the recover table around a breaker?
  return db()->Execute(recover_drop.c_str());
}

bool Recovery::SetupMeta() {
  // clang-format off
  static const char kCreateSql[] =
      "CREATE VIRTUAL TABLE temp.recover_meta USING recover("
         "corrupt.meta,"
         "key TEXT NOT NULL,"
         "value ANY"  // Whatever is stored.
      ")";
  // clang-format on
  return db()->Execute(kCreateSql);
}

bool Recovery::GetMetaVersionNumber(int* version) {
  DCHECK(version);
  // TODO(shess): DCHECK(db()->DoesTableExist("temp.recover_meta"));
  // Unfortunately, DoesTableExist() queries sqlite_schema, not
  // sqlite_temp_master.

  static const char kVersionSql[] =
      "SELECT value FROM temp.recover_meta WHERE key = 'version'";
  sql::Statement recovery_version(db()->GetUniqueStatement(kVersionSql));
  if (!recovery_version.Step())
    return false;

  *version = recovery_version.ColumnInt(0);
  return true;
}

namespace {

// Collect statements from [corrupt.sqlite_schema.sql] which start with |prefix|
// (which should be a valid SQL string ending with the space before a table
// name), then apply the statements to [main].  Skip any table named
// 'sqlite_sequence', as that table is created on demand by SQLite if any tables
// use AUTOINCREMENT.
//
// Returns |true| if all of the matching items were created in the main
// database.  Returns |false| if an item fails on creation, or if the corrupt
// database schema cannot be queried.
bool SchemaCopyHelper(Database* db, const char* prefix) {
  const size_t prefix_len = strlen(prefix);
  DCHECK_EQ(' ', prefix[prefix_len-1]);

  sql::Statement s(db->GetUniqueStatement(
      "SELECT DISTINCT sql FROM corrupt.sqlite_schema "
      "WHERE name<>'sqlite_sequence'"));
  while (s.Step()) {
    std::string sql = s.ColumnString(0);

    // Skip statements that don't start with |prefix|.
    if (sql.compare(0, prefix_len, prefix) != 0)
      continue;

    sql.insert(prefix_len, "main.");
    if (!db->Execute(sql.c_str()))
      return false;
  }
  return s.Succeeded();
}

}  // namespace

// This method is derived from SQLite's vacuum.c.  VACUUM operates very
// similarily, creating a new database, populating the schema, then copying the
// data.
//
// TODO(shess): This conservatively uses Rollback() rather than Unrecoverable().
// With Rollback(), it is expected that the database will continue to generate
// errors. Change the failure cases to Unrecoverable().
//
// static
std::unique_ptr<Recovery> Recovery::BeginRecoverDatabase(
    Database* db,
    const base::FilePath& db_path) {
  std::unique_ptr<sql::Recovery> recovery = sql::Recovery::Begin(db, db_path);
  if (!recovery) {
    // Close the underlying sqlite* handle.  Windows does not allow deleting
    // open files, and all platforms block opening a second sqlite3* handle
    // against a database when exclusive locking is set.
    db->Poison();

    // When this code was written, histograms showed that most failures happened
    // while attaching a corrupt database. In this case, a large proportion of
    // attachment failures were SQLITE_NOTADB.
    //
    // We currently only delete the database in that specific failure case.
    {
      Database probe_db;
      if (!probe_db.OpenInMemory() ||
          probe_db.AttachDatabase(db_path, "corrupt", InternalApiToken()) ||
          probe_db.GetErrorCode() != SQLITE_NOTADB) {
        return nullptr;
      }
    }

    // The database has invalid data in the SQLite header, so it is almost
    // certainly not recoverable without manual intervention (and likely not
    // recoverable _with_ manual intervention).  Clear away the broken database.
    if (!sql::Database::Delete(db_path))
      return nullptr;

    // Windows deletion is complicated by file scanners and malware - sometimes
    // Delete() appears to succeed, even though the file remains.  The following
    // attempts to track if this happens often enough to cause concern.
    {
      Database probe_db;
      if (!probe_db.Open(db_path))
        return nullptr;

      if (!probe_db.Execute("PRAGMA auto_vacuum"))
        return nullptr;
    }

    // The rest of the recovery code could be run on the re-opened database, but
    // the database is empty, so there would be no point.
    return nullptr;
  }

#if DCHECK_IS_ON()
  // This code silently fails to recover fts3 virtual tables.  At this time no
  // browser database contain fts3 tables.  Just to be safe, complain loudly if
  // the database contains virtual tables.
  //
  // fts3 has an [x_segdir] table containing a column [end_block INTEGER].  But
  // it actually stores either an integer or a text containing a pair of
  // integers separated by a space.  AutoRecoverTable() trusts the INTEGER tag
  // when setting up the recover vtable, so those rows get dropped.  Setting
  // that column to ANY may work.
  if (db->is_open()) {
    sql::Statement s(db->GetUniqueStatement(
        "SELECT 1 FROM sqlite_schema WHERE sql LIKE 'CREATE VIRTUAL TABLE %'"));
    DCHECK(!s.Step()) << "Recovery of virtual tables not supported";
  }
#endif

  // TODO(shess): vacuum.c turns off checks and foreign keys.

  // TODO(shess): vacuum.c turns synchronous=OFF for the target.  I do not fully
  // understand this, as the temporary db should not have a journal file at all.
  // Perhaps it does in case of cache spill?

  // Copy table schema from [corrupt] to [main].
  if (!SchemaCopyHelper(recovery->db(), "CREATE TABLE ") ||
      !SchemaCopyHelper(recovery->db(), "CREATE INDEX ") ||
      !SchemaCopyHelper(recovery->db(), "CREATE UNIQUE INDEX ")) {
    // No RecordRecoveryEvent() here because SchemaCopyHelper() already did.
    Recovery::Rollback(std::move(recovery));
    return nullptr;
  }

  // Run auto-recover against each table, skipping the sequence table.  This is
  // necessary because table recovery can create the sequence table as a side
  // effect, so recovering that table inline could lead to duplicate data.
  {
    sql::Statement s(recovery->db()->GetUniqueStatement(
        "SELECT name FROM sqlite_schema WHERE sql LIKE 'CREATE TABLE %' "
        "AND name!='sqlite_sequence'"));
    while (s.Step()) {
      const std::string name = s.ColumnString(0);
      size_t rows_recovered;
      if (!recovery->AutoRecoverTable(name.c_str(), &rows_recovered)) {
        Recovery::Rollback(std::move(recovery));
        return nullptr;
      }
    }
    if (!s.Succeeded()) {
      Recovery::Rollback(std::move(recovery));
      return nullptr;
    }
  }

  // Overwrite any sequences created.
  if (recovery->db()->DoesTableExist("corrupt.sqlite_sequence")) {
    std::ignore = recovery->db()->Execute("DELETE FROM main.sqlite_sequence");
    size_t rows_recovered;
    if (!recovery->AutoRecoverTable("sqlite_sequence", &rows_recovered)) {
      Recovery::Rollback(std::move(recovery));
      return nullptr;
    }
  }

  // Copy triggers and views directly to sqlite_schema.  Any tables they refer
  // to should already exist.
  static const char kCreateMetaItemsSql[] =
      "INSERT INTO main.sqlite_schema "
      "SELECT type, name, tbl_name, rootpage, sql "
      "FROM corrupt.sqlite_schema WHERE type='view' OR type='trigger'";
  if (!recovery->db()->Execute(kCreateMetaItemsSql)) {
    Recovery::Rollback(std::move(recovery));
    return nullptr;
  }

  return recovery;
}

void Recovery::RecoverDatabase(Database* db, const base::FilePath& db_path) {
  std::unique_ptr<sql::Recovery> recovery = BeginRecoverDatabase(db, db_path);

  if (recovery)
    std::ignore = Recovery::Recovered(std::move(recovery));
}

void Recovery::RecoverDatabaseWithMetaVersion(Database* db,
                                              const base::FilePath& db_path) {
  std::unique_ptr<sql::Recovery> recovery = BeginRecoverDatabase(db, db_path);
  if (!recovery)
    return;

  int version = 0;
  if (!recovery->SetupMeta() || !recovery->GetMetaVersionNumber(&version)) {
    sql::Recovery::Unrecoverable(std::move(recovery));
    return;
  }

  std::ignore = Recovery::Recovered(std::move(recovery));
}

// static
bool Recovery::ShouldRecover(int extended_error) {
  // Trim extended error codes.
  int error = extended_error & 0xFF;
  switch (error) {
    case SQLITE_NOTADB:
      // SQLITE_NOTADB happens if the SQLite header is broken.  Some earlier
      // versions of SQLite return this where other versions return
      // SQLITE_CORRUPT, which is a recoverable case.  Later versions only
      // return this error only in unrecoverable cases, in which case recovery
      // will fail with no changes to the database, so there's no harm in
      // attempting recovery in this case.
      return true;

    case SQLITE_CORRUPT:
      // SQLITE_CORRUPT generally means that the database is readable as a
      // SQLite database, but some inconsistency has been detected by SQLite.
      // In many cases the inconsistency is relatively trivial, such as if an
      // index refers to a row which was deleted, in which case most or even all
      // of the data can be recovered.  This can also be reported if parts of
      // the file have been overwritten with garbage data, in which recovery
      // should be able to recover partial data.
      return true;

      // TODO(shess): Possible future options for automated fixing:
      // - SQLITE_CANTOPEN - delete the broken symlink or directory.
      // - SQLITE_PERM - permissions could be fixed.
      // - SQLITE_READONLY - permissions could be fixed.
      // - SQLITE_IOERR - rewrite using new blocks.
      // - SQLITE_FULL - recover in memory and rewrite subset of data.

    default:
      return false;
  }
}

// static
int Recovery::EnableRecoveryExtension(Database* db, InternalApiToken) {
  return sql::recover::RegisterRecoverExtension(db->db(InternalApiToken()));
}

}  // namespace sql
