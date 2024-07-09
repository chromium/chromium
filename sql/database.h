// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_DATABASE_H_
#define SQL_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "sql/internal_api_token.h"
#include "sql/sql_features.h"
#include "sql/sqlite_result_code.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement_id.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"

// Forward declaration for SQLite structures. Headers in the public sql:: API
// must NOT include sqlite3.h.
struct sqlite3;
struct sqlite3_file;
struct sqlite3_stmt;

namespace base::trace_event {
class ProcessMemoryDump;
}  // namespace base::trace_event

namespace perfetto::protos::pbzero {
class ChromeSqlDiagnostics;
}

namespace sql {

class DatabaseMemoryDumpProvider;
class Statement;

namespace test {
class ScopedErrorExpecter;
}  // namespace test

struct COMPONENT_EXPORT(SQL) DatabaseOptions {
  // Default page size for newly created databases.
  //
  // Guaranteed to match SQLITE_DEFAULT_PAGE_SIZE.
  static constexpr int kDefaultPageSize = 4096;

  // If true, the database can only be opened by one process at a time.
  //
  // SQLite supports a locking protocol that allows multiple processes to safely
  // operate on the same database at the same time. The locking protocol is used
  // on every transaction, and comes with a small performance penalty.
  //
  // Setting this to true causes the locking protocol to be used once, when the
  // database is opened. No other SQLite process will be able to access the
  // database at the same time. Note that this uses OS-level
  // advisory/cooperative locking, so this does not protect the database file
  // from uncooperative processes.
  //
  // More details at https://www.sqlite.org/pragma.html#pragma_locking_mode
  //
  // SQLite's locking protocol is summarized at
  // https://www.sqlite.org/c3ref/io_methods.html
  //
  // Exclusive mode is strongly recommended. It reduces the I/O cost of setting
  // up a transaction. It also removes the need of handling transaction failures
  // due to lock contention.
  bool exclusive_locking = true;

  // If true, enables exclusive=true vfs URI parameter on the database file.
  // This is only supported on Windows.
  //
  // If this option is true then the database file cannot be opened by any
  // processes on the system until the database has been closed. Note, this is
  // not the same as `exclusive_locking` above, which refers to
  // advisory/cooperative locks. This option sets file handle sharing attributes
  // to prevent the database files from being opened from any process including
  // being opened a second time by the hosting process.
  //
  // A side effect of setting this flag is that the database cannot be
  // preloaded. If you would like to set this flag on a preloaded database,
  // please reach out to a //sql owner.
  //
  // This option is experimental and will be merged into the `exclusive_locking`
  // option above if proven to cause no OS compatibility issues.
  // TODO(crbug.com/40262539): Merge into above option, if possible.
  bool exclusive_database_file_lock = false;

  // If true, enables SQLite's Write-Ahead Logging (WAL).
  //
  // WAL integration is under development, and should not be used in shipping
  // Chrome features yet.
  //
  // WAL mode is currently not fully supported on FuchsiaOS. It will only be
  // turned on if the database is also using exclusive locking mode.
  // (https://crbug.com/1082059)
  //
  // Note: Changing page size is not supported when in WAL mode. So running
  // 'PRAGMA page_size = <new-size>' will result in no-ops.
  //
  // More details at https://www.sqlite.org/wal.html
  bool wal_mode =
      base::FeatureList::IsEnabled(sql::features::kEnableWALModeByDefault);

  // If true, transaction commit waits for data to reach persistent media.
  //
  // This is currently only meaningful on macOS. All other operating systems
  // only support flushing directly to disk.
  //
  // If both `flush_to_media` and `wal_mode` are false, power loss can lead to
  // database corruption.
  //
  // By default, SQLite considers that transactions commit when they reach the
  // disk controller's memory. This guarantees durability in the event of
  // software crashes, up to and including the operating system. In the event of
  // power loss, SQLite may lose data. If `wal_mode` is false (SQLite uses a
  // rollback journal), power loss can lead to database corruption.
  //
  // When this option is enabled, committing a transaction causes SQLite to wait
  // until the data is written to the persistent media. This guarantees
  // durability in the event of power loss, which is needed to guarantee the
  // integrity of non-WAL databases.
  bool flush_to_media = false;

  // Database page size.
  //
  // New Chrome features should set an explicit page size in their
  // DatabaseOptions initializers, even if they use the default page size. This
  // makes it easier to track the page size used by the databases on the users'
  // devices.
  //
  // The value in this option is only applied to newly created databases. In
  // other words, changing the value doesn't impact the databases that have
  // already been created on the users' devices. So, changing the value in the
  // code without a lot of work (re-creating existing databases) will result in
  // inconsistent page sizes across the fleet of user devices, which will make
  // it (even) more difficult to reason about database performance.
  //
  // Larger page sizes result in shallower B-trees, because they allow an inner
  // page to hold more keys. On the flip side, larger page sizes may result in
  // more I/O when making small changes to existing records.
  //
  // Must be a power of two between 512 and 65536 inclusive.
  //
  // TODO(pwnall): Replace the default with an invalid value after all
  //               sql::Database users explicitly initialize page_size.
  int page_size = kDefaultPageSize;

  // The size of in-memory cache, in pages.
  //
  // New Chrome features should set an explicit cache size in their
  // DatabaseOptions initializers, even if they use the default cache size. This
  // makes it easier to track the cache size used by the databases on the users'
  // devices. The default page size of 4,096 bytes results in a cache size of
  // 500 pages.
  //
  // SQLite's database cache will take up at most (`page_size` * `cache_size`)
  // bytes of RAM.
  //
  // 0 invokes SQLite's default, which is currently to size up the cache to use
  // exactly 2,048,000 bytes of RAM.
  //
  // TODO(pwnall): Replace the default with an invalid value after all
  //               sql::Database users explicitly initialize page_size.
  int cache_size = 0;

  // Stores mmap failures in the SQL schema, instead of the meta table.
  //
  // This option is strongly discouraged for new databases, and will eventually
  // be removed.
  //
  // If this option is true, the mmap status is stored in the database schema.
  // Like any other schema change, changing the mmap status invalidates all
  // pre-compiled SQL statements.
  bool mmap_alt_status_discouraged = false;

  // If true, enables SQL views (a discouraged feature) for this database.
  //
  // The use of views is discouraged for Chrome code. See README.md for details
  // and recommended replacements.
  //
  // If this option is false, CREATE VIEW and DROP VIEW succeed, but SELECT
  // statements targeting views fail.
  bool enable_views_discouraged = false;
};

// Holds database diagnostics in a structured format.
struct COMPONENT_EXPORT(SQL) DatabaseDiagnostics {
  DatabaseDiagnostics();
  ~DatabaseDiagnostics();

  using TraceProto = perfetto::protos::pbzero::ChromeSqlDiagnostics;
  // Write a representation of this object into tracing proto.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const;

  // This was the original error code that triggered the error callback. Should
  // generally match `error_code`, but this isn't guaranteed by the code.
  int reported_sqlite_error_code = 0;

  // Corresponds to `Database::GetErrorCode()`.
  int error_code = 0;

  // Corresponds to `Database::GetLastErrno()`.
  int last_errno = 0;

  // Corresponds to `Statement::GetSQLStatement()` of the problematic statement.
  // This doesn't include the bound values, and therefore is free of any PII.
  std::string sql_statement;

  // The 'version' value stored in the user database's meta table, if it can be
  // read. If we fail to read the version of the user database, it's left as 0.
  int version = 0;

  // Most rows in 'sql_schema' have a non-NULL 'sql' column. Those rows' 'sql'
  // contents are logged here, one element per row.
  std::vector<std::string> schema_sql_rows;

  // Some rows of 'sql_schema' have a NULL 'sql' column. They are typically
  // autogenerated indices, like "sqlite_autoindex_downloads_slices_1". These
  // are also logged here by their 'name' column, one element per row.
  std::vector<std::string> schema_other_row_names;

  // Sanity checks used for all errors.
  bool has_valid_header = false;
  bool has_valid_schema = false;

  // Corresponds to `Database::GetErrorMessage()`.
  std::string error_message;
};

// Handle to an open SQLite database.
//
// Instances of this class are not thread-safe. With few exceptions, Database
// instances should only be accessed from one sequence. Database instances may
// be constructed on one sequence and safely used/destroyed on another. Callers
// may explicitly use `DetachFromSequence()` before moving to another sequence.
//
// When a Database instance goes out of scope, any uncommitted transactions are
// rolled back.
class COMPONENT_EXPORT(SQL) Database {
 private:
  class StatementRef;  // Forward declaration, see real one below.

 public:
  // Creates an instance that can receive Open() / OpenInMemory() calls.
  //
  // Some `options` members are only applied to newly created databases.
  //
  // Most operations on the new instance will fail until Open() / OpenInMemory()
  // is called.
  explicit Database(DatabaseOptions options);

  // This constructor is deprecated.
  //
  // When transitioning away from this default constructor, consider setting
  // DatabaseOptions::explicit_locking to true. For historical reasons, this
  // constructor results in DatabaseOptions::explicit_locking set to false.
  //
  // TODO(crbug.com/40148370): Remove this constructor after migrating all
  //                          uses to the explicit constructor below.
  Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(Database&&) = delete;
  ~Database();

  // Allows mmapping to be disabled globally by default in the calling process.
  // Must be called before any threads attempt to create a Database.
  //
  // TODO(crbug.com/40144971): Remove this global configuration.
  static void DisableMmapByDefault();

  // Pre-init configuration ----------------------------------------------------

  // The page size that will be used when creating a new database.
  int page_size() const { return options_.page_size; }

  // Returns whether a database will be opened in WAL mode.
  bool UseWALMode() const;

  // Opt out of memory-mapped file I/O.
  void set_mmap_disabled() { mmap_disabled_ = true; }

  // Set an error-handling callback.  On errors, the error number (and
  // statement, if available) will be passed to the callback.
  //
  // If no callback is set, the default error-handling behavior is invoked. The
  // default behavior is to LOGs the error and propagate the failure.
  //
  // In DCHECK-enabled builds, the default error-handling behavior currently
  // DCHECKs on errors. This is not correct, because DCHECKs are supposed to
  // cover invariants and never fail, whereas SQLite errors can surface even on
  // correct usage, due to I/O errors and data corruption. At some point in the
  // future, errors will not result in DCHECKs.
  //
  // The callback will be called on the sequence used for database operations.
  // The callback will never be called after the Database instance is destroyed.
  using ErrorCallback = base::RepeatingCallback<void(int, Statement*)>;
  void set_error_callback(ErrorCallback callback) {
    DCHECK(!callback.is_null()) << "Use reset_error_callback() explicitly";
    DCHECK(error_callback_.is_null())
        << "Overwriting previously set error callback";
    error_callback_ = std::move(callback);
  }
  void reset_error_callback() { error_callback_.Reset(); }
  bool has_error_callback() const { return !error_callback_.is_null(); }

  // Developer-friendly database ID used in logging output and memory dumps.
  void set_histogram_tag(const std::string& histogram_tag) {
    DCHECK(!is_open());
    histogram_tag_ = histogram_tag;
  }

  const std::string& histogram_tag() const { return histogram_tag_; }

  // Asks SQLite to perform a full integrity check on the database.
  //
  // Returns true if the integrity check was completed successfully. Success
  // does not necessarily entail that the database is healthy. Finding
  // corruption and reporting it in `messages` counts as success.
  //
  // If the method returns true, `messages` is populated with a list of
  // diagnostic messages. If the integrity check finds no errors, `messages`
  // will contain exactly one "ok" string. This unusual API design is explained
  // by the fact that SQLite exposes integrity check functionality as a PRAGMA,
  // and the PRAGMA returns "ok" in case of success.
  bool FullIntegrityCheck(std::vector<std::string>* messages);

  // Meant to be called from a client error callback so that it's able to
  // get diagnostic information about the database. `diagnostics` is an optional
  // out parameter. If `diagnostics` is defined, this method populates all of
  // its fields.
  std::string GetDiagnosticInfo(int extended_error,
                                Statement* statement,
                                DatabaseDiagnostics* diagnostics = nullptr);

  // Reports memory usage into provided memory dump with the given name.
  bool ReportMemoryUsage(base::trace_event::ProcessMemoryDump* pmd,
                         const std::string& dump_name);

  // Initialization ------------------------------------------------------------

  // Opens or creates a database on disk.
  //
  // `db_file_path` points to the file storing database pages. Other files
  // associated with the database (rollback journal, write-ahead log,
  // shared-memory file) may be created.
  //
  // Returns true in case of success, false in case of failure. If an error
  // occurs, this function will invoke the error callback if it is present and
  // then may attempt to open the database a second time. If the second attempt
  // succeeds, it will return true.
  [[nodiscard]] bool Open(const base::FilePath& db_file_path);

  // Alternative to Open() that creates an in-memory database.
  //
  // Returns true in case of success, false in case of failure.
  //
  // The memory associated with the database will be released when the database
  // is closed.
  [[nodiscard]] bool OpenInMemory();

  // Returns true if the database has been successfully opened.
  bool is_open() const;

  // Detach from the currently-attached sequence. If already attached to a
  // sequence, this method must be called from that sequence.
  void DetachFromSequence();

  // Closes the database. This is automatically performed on destruction for
  // you, but this allows you to close the database early. You must not call
  // any other functions after closing it. It is permissable to call Close on
  // an uninitialized or already-closed database.
  void Close();

  // Hints the file system that the database will be accessed soon.
  //
  // This method should be called on databases that are on the critical path to
  // Chrome startup. Informing the filesystem about our expected access pattern
  // early on reduces the likelihood that we'll be blocked on disk I/O. This has
  // a high impact on startup time.
  //
  // This method should not be used for non-critical databases. While using it
  // will likely improve micro-benchmarks involving one specific database,
  // overuse risks randomizing the disk I/O scheduler, slowing down Chrome
  // startup.
  void Preload();

  // Release all non-essential memory associated with this database connection.
  void TrimMemory();

  // Raze the database to the ground.  This approximates creating a
  // fresh database from scratch, within the constraints of SQLite's
  // locking protocol (locks and open handles can make doing this with
  // filesystem operations problematic).  Returns true if the database
  // was razed.
  //
  // false is returned if the database is locked by some other
  // process.
  //
  // NOTE(shess): Raze() will DCHECK in the following situations:
  // - database is not open.
  // - the database has a transaction open.
  // - a SQLite issue occurs which is structural in nature (like the
  //   statements used are broken).
  // Since Raze() is expected to be called in unexpected situations,
  // these all return false, since it is unlikely that the caller
  // could fix them.
  //
  // The database's page size is taken from |options_.page_size|.  The
  // existing database's |auto_vacuum| setting is lost (the
  // possibility of corruption makes it unreliable to pull it from the
  // existing database).  To re-enable on the empty database requires
  // running "PRAGMA auto_vacuum = 1;" then "VACUUM".
  //
  // NOTE(shess): For Android, SQLITE_DEFAULT_AUTOVACUUM is set to 1,
  // so Raze() sets auto_vacuum to 1.
  //
  // TODO(shess): Raze() needs a database so cannot clear SQLITE_NOTADB.
  // TODO(shess): Bake auto_vacuum into Database's API so it can
  // just pick up the default.
  bool Raze();

  // Breaks all outstanding transactions (as initiated by
  // BeginTransaction()), closes the SQLite database, and poisons the
  // object so that all future operations against the Database (or
  // its Statements) fail safely, without side effects.
  //
  // This is intended as an alternative to Close() in error callbacks.
  // Close() should still be called at some point.
  void Poison();

  // `Raze()` the database and `Poison()` the handle. Returns the return
  // value from `Raze()`.
  bool RazeAndPoison();

  // Delete the underlying database files associated with |path|. This should be
  // used on a database which is not opened by any Database instance. Open
  // Database instances pointing to the database can cause odd results or
  // corruption (for instance if a hot journal is deleted but the associated
  // database is not).
  //
  // Returns true if the database file and associated journals no
  // longer exist, false otherwise.  If the database has never
  // existed, this will return true.
  static bool Delete(const base::FilePath& path);

  // Transactions --------------------------------------------------------------

  // Transaction management. We maintain a virtual transaction stack to emulate
  // nested transactions since sqlite can't do nested transactions. The
  // limitation is you can't roll back a sub transaction: if any transaction
  // fails, all transactions open will also be rolled back. Any nested
  // transactions after one has rolled back will return fail for Begin(). If
  // Begin() fails, you must not call Commit or Rollback().
  //
  // Normally you should use sql::Transaction to manage a transaction, which
  // will scope it to a C++ context.
  [[nodiscard]] bool BeginTransaction(InternalApiToken);
  void RollbackTransaction(InternalApiToken);
  [[nodiscard]] bool CommitTransaction(InternalApiToken);

  // These methods are deprecated and will be removed in the future: The
  // `Transaction` class should be used instead.
  bool BeginTransactionDeprecated();
  void RollbackTransactionDeprecated();
  bool CommitTransactionDeprecated();

  // Rollback all outstanding transactions.  Use with care, there may
  // be scoped transactions on the stack.
  void RollbackAllTransactions();

  bool HasActiveTransactions() const {
    DCHECK_GE(transaction_nesting_, 0);
    return is_open() && transaction_nesting_ > 0;
  }

  // Deprecated in favor of HasActiveTransactions().
  //
  // Returns the current transaction nesting, which will be 0 if there are
  // no open transactions.
  int transaction_nesting() const { return transaction_nesting_; }

  // Attached databases---------------------------------------------------------

  // Attaches an existing database to this connection.
  //
  // `attachment_point` must only contain lowercase letters.
  //
  // Use is generally discouraged in production code. The README has more
  // details.
  //
  // On the SQLite version shipped with Chrome (3.21+, Oct 2017), databases can
  // be attached while a transaction is opened. However, these databases cannot
  // be detached until the transaction is committed or aborted.
  bool AttachDatabase(const base::FilePath& other_db_path,
                      std::string_view attachment_point);

  // Detaches a database that was previously attached with AttachDatabase().
  //
  // `attachment_point` must match the argument of a previously successsful
  // AttachDatabase() call.
  //
  // Attachment APIs are only exposed for use in recovery. General use is
  // discouraged in Chrome. The README has more details.
  bool DetachDatabase(std::string_view attachment_point);

  // Statements ----------------------------------------------------------------

  // Executes a SQL statement. Returns true for success, and false for failure.
  //
  // `sql` should be a single SQL statement. Production code should not execute
  // multiple SQL statements at once, to facilitate crash debugging. Test code
  // should use ExecuteScriptForTesting().
  //
  // `sql` cannot have parameters. Statements with parameters can be handled by
  // sql::Statement. See GetCachedStatement() and GetUniqueStatement().
  [[nodiscard]] bool Execute(base::cstring_view sql);

  // Executes a sequence of SQL statements.
  //
  // Returns true if all statements execute successfully. If a statement fails,
  // stops and returns false. Calls should be wrapped in ASSERT_TRUE().
  //
  // The database's error handler is not invoked when errors occur. This method
  // is a convenience for setting up a complex on-disk database state, such as
  // an old schema version with test contents.
  [[nodiscard]] bool ExecuteScriptForTesting(base::cstring_view sql_script);

  // Returns a statement for the given SQL using the statement cache. It can
  // take a nontrivial amount of work to parse and compile a statement, so
  // keeping commonly-used ones around for future use is important for
  // performance.
  //
  // The SQL_FROM_HERE macro is the recommended way of generating a StatementID.
  // Code that generates custom IDs must ensure that a StatementID is never used
  // for different SQL statements. Failing to meet this requirement results in
  // incorrect behavior, and should be caught by a DCHECK.
  //
  // The SQL statement passed in |sql| must match the SQL statement reported
  // back by SQLite. Mismatches are caught by a DCHECK, so any code that has
  // automated test coverage or that was manually tested on a DCHECK build will
  // not exhibit this problem. Mismatches generally imply that the statement
  // passed in has extra whitespace or comments surrounding it, which waste
  // storage and CPU cycles.
  //
  // If the |sql| has an error, an invalid, inert StatementRef is returned (and
  // the code will crash in debug). The caller must deal with this eventuality,
  // either by checking validity of the |sql| before calling, by correctly
  // handling the return of an inert statement, or both.
  //
  // Example:
  //   sql::Statement stmt(database_.GetCachedStatement(
  //       SQL_FROM_HERE, "SELECT * FROM foo"));
  //   if (!stmt)
  //     return false;  // Error creating statement.
  scoped_refptr<StatementRef> GetCachedStatement(StatementID id,
                                                 base::cstring_view sql);

  // Used to check a |sql| statement for syntactic validity. If the statement is
  // valid SQL, returns true.
  bool IsSQLValid(base::cstring_view sql);

  // Returns a non-cached statement for the given SQL. Use this for SQL that
  // is only executed once or only rarely (there is overhead associated with
  // keeping a statement cached).
  //
  // See GetCachedStatement above for examples and error information.
  scoped_refptr<StatementRef> GetUniqueStatement(base::cstring_view sql);

  // Returns a non-cached statement same as `GetUniqueStatement()`, except
  // returns an invalid statement if the statement makes direct changes to the
  // database file. This readonly check does not include changes made by
  // application-defined functions. See more at:
  // https://www.sqlite.org/c3ref/stmt_readonly.html.
  scoped_refptr<Database::StatementRef> GetReadonlyStatement(
      base::cstring_view sql);

  // Performs a passive checkpoint on the main attached database if it is in
  // WAL mode. Returns true if the checkpoint was successful and false in case
  // of an error. It is a no-op if the database is not in WAL mode.
  //
  // Note: Checkpointing is a very slow operation and will block any writes
  // until it is finished. Please use with care.
  bool CheckpointDatabase();

  // Info querying -------------------------------------------------------------

  // Returns true if the given structure exists.  Instead of test-then-create,
  // callers should almost always prefer the "IF NOT EXISTS" version of the
  // CREATE statement.
  // TODO(https://crbug.com/341639215): these should take a `base::cstring`.
  bool DoesIndexExist(std::string_view index_name);
  bool DoesTableExist(std::string_view table_name);
  bool DoesViewExist(std::string_view table_name);

  // Returns true if a column with the given name exists in the given table.
  //
  // Calling this method on a VIEW returns an unspecified result.
  //
  // This should only be used by migration code for legacy features that do not
  // use MetaTable, and need an alternative way of figuring out the database's
  // current version.
  bool DoesColumnExist(base::cstring_view table_name,
                       base::cstring_view column_name);

  // Returns sqlite's internal ID for the last inserted row. Valid only
  // immediately after an insert.
  int64_t GetLastInsertRowId() const;

  // Returns sqlite's count of the number of rows modified by the last
  // statement executed. Will be 0 if no statement has executed or the database
  // is closed.
  int64_t GetLastChangeCount();

  // Approximates the amount of memory used by SQLite for this database.
  //
  // This measures the memory used for the page cache (most likely the biggest
  // consumer), database schema, and prepared statements.
  //
  // The memory used by the page cache can be recovered by calling TrimMemory(),
  // which will cause SQLite to drop the page cache.
  int GetMemoryUsage();

  // Errors --------------------------------------------------------------------

  // Returns the error code associated with the last sqlite operation.
  int GetErrorCode() const;

  // Returns the errno associated with GetErrorCode(). See <errno.h>.
  int GetLastErrno() const;

  // Returns a pointer to a statically allocated string associated with the
  // last sqlite operation.
  const char* GetErrorMessage() const;

  // Return a reproducible representation of the schema equivalent to
  // running the following statement at a sqlite3 command-line:
  //   SELECT type, name, tbl_name, sql FROM sqlite_schema ORDER BY 1, 2, 3, 4;
  std::string GetSchema();

  // Returns |true| if there is an error expecter (see SetErrorExpecter), and
  // that expecter returns |true| when passed |error|.  Clients which provide an
  // |error_callback| should use IsExpectedSqliteError() to check for unexpected
  // errors; if one is detected, DLOG(FATAL) is generally appropriate (see
  // OnSqliteError implementation).
  static bool IsExpectedSqliteError(int sqlite_error_code);

  // Computes the path of a database's rollback journal.
  //
  // The journal file is created at the beginning of the database's first
  // transaction. The file may be removed and re-created between transactions,
  // depending on whether the database is opened in exclusive mode, and on
  // configuration options. The journal file does not exist when the database
  // operates in WAL mode.
  //
  // This is intended for internal use and tests. To preserve our ability to
  // iterate on our SQLite configuration, features must avoid relying on
  // the existence of specific files.
  static base::FilePath JournalPath(const base::FilePath& db_path);

  // Computes the path of a database's write-ahead log (WAL).
  //
  // The WAL file exists while a database is opened in WAL mode.
  //
  // This is intended for internal use and tests. To preserve our ability to
  // iterate on our SQLite configuration, features must avoid relying on
  // the existence of specific files.
  static base::FilePath WriteAheadLogPath(const base::FilePath& db_path);

  // Computes the path of a database's shared memory (SHM) file.
  //
  // The SHM file is used to coordinate between multiple processes using the
  // same database in WAL mode. Thus, this file only exists for databases using
  // WAL and not opened in exclusive mode.
  //
  // This is intended for internal use and tests. To preserve our ability to
  // iterate on our SQLite configuration, features must avoid relying on
  // the existence of specific files.
  static base::FilePath SharedMemoryFilePath(const base::FilePath& db_path);

  // Internal state accessed by other classes in //sql.
  base::WeakPtr<Database> GetWeakPtr(InternalApiToken);
  sqlite3* db(InternalApiToken) const { return db_; }
  bool poisoned(InternalApiToken) const { return poisoned_; }
  base::FilePath DbPath(InternalApiToken) const { return DbPath(); }

  // Interface with sql::test::ScopedErrorExpecter.
  using ScopedErrorExpecterCallback = base::RepeatingCallback<bool(int)>;
  static void SetScopedErrorExpecter(ScopedErrorExpecterCallback* expecter,
                                     base::PassKey<test::ScopedErrorExpecter>);
  static void ResetScopedErrorExpecter(
      base::PassKey<test::ScopedErrorExpecter>);

 private:
  // Statement accesses StatementRef which we don't want to expose to everybody
  // (they should go through Statement).
  friend class Statement;

  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, CachedStatement);
  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, CollectDiagnosticInfo);
  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, ComputeMmapSizeForOpen);
  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, ComputeMmapSizeForOpenAltStatus);
  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, OnMemoryDump);
  FRIEND_TEST_ALL_PREFIXES(SQLDatabaseTest, RegisterIntentToUpload);
  FRIEND_TEST_ALL_PREFIXES(SQLiteFeaturesTest, FTS3_Prefix);
  FRIEND_TEST_ALL_PREFIXES(SQLiteFeaturesTest, WALNoClose);
  FRIEND_TEST_ALL_PREFIXES(SQLEmptyPathDatabaseTest, EmptyPathTest);

  // Implements Open(), OpenInMemory().
  //
  // `db_file_path` is a UTF-8 path to the file storing the database pages. If
  // `file_name` is the SQLite magic memory path :memory:, the database will be
  // opened in-memory.
  bool OpenInternal(const std::string& file_name);

  // Configures the underlying sqlite3* object via sqlite3_db_config().
  //
  // To minimize the number of possible SQLite code paths executed in Chrome,
  // this method must be called right after the underlying sqlite3* object is
  // obtained from sqlite3_open*(), before any other sqlite3_*() methods are
  // called on the object.
  void ConfigureSqliteDatabaseObject();

  // Internal close function used by Close() and RazeAndPoison().
  // |forced| indicates that orderly-shutdown checks should not apply.
  void CloseInternal(bool forced);

  // Construct a ScopedBlockingCall to annotate IO calls, but only if
  // database wasn't open in memory. ScopedBlockingCall uses |from_here| to
  // declare its blocking execution scope (see https://www.crbug/934302).
  void InitScopedBlockingCall(
      const base::Location& from_here,
      std::optional<base::ScopedBlockingCall>* scoped_blocking_call) const {
    if (!in_memory_)
      scoped_blocking_call->emplace(from_here, base::BlockingType::MAY_BLOCK);
  }

  // Internal helper for Does*Exist() functions.
  bool DoesSchemaItemExist(std::string_view name, std::string_view type);

  // Used to implement the interface with sql::test::ScopedErrorExpecter.
  static ScopedErrorExpecterCallback* current_expecter_cb_;

  // A StatementRef is a refcounted wrapper around a sqlite statement pointer.
  // Refcounting allows us to give these statements out to sql::Statement
  // objects while also optionally maintaining a cache of compiled statements
  // by just keeping a refptr to these objects.
  //
  // A statement ref can be valid, in which case it can be used, or invalid to
  // indicate that the statement hasn't been created yet, has an error, or has
  // been destroyed.
  //
  // The Database may revoke a StatementRef in some error cases, so callers
  // should always check validity before using.
  class COMPONENT_EXPORT(SQL) StatementRef
      : public base::RefCounted<StatementRef> {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

    // |database| is the sql::Database instance associated with
    // the statement, and is used for tracking outstanding statements
    // and for error handling.  Set to nullptr for invalid refs.
    // |stmt| is the actual statement, and should only be null
    // to create an invalid ref.  |was_valid| indicates whether the
    // statement should be considered valid for diagnostic purposes.
    // |was_valid| can be true for a null |stmt| if the Database has
    // been forcibly closed by an error handler.
    StatementRef(Database* database, sqlite3_stmt* stmt, bool was_valid);

    StatementRef(const StatementRef&) = delete;
    StatementRef& operator=(const StatementRef&) = delete;
    StatementRef(StatementRef&&) = delete;
    StatementRef& operator=(StatementRef&&) = delete;

    // When true, the statement can be used.
    bool is_valid() const { return !!stmt_; }

    // When true, the statement is either currently valid, or was
    // previously valid but the database was forcibly closed.  Used
    // for diagnostic checks.
    bool was_valid() const { return was_valid_; }

    // If we've not been linked to a database, this will be null.
    Database* database() const { return database_; }

    // Returns the sqlite statement if any. If the statement is not active,
    // this will return nullptr.
    sqlite3_stmt* stmt() const { return stmt_; }

    // Destroys the compiled statement and sets it to nullptr. The statement
    // will no longer be active. |forced| is used to indicate if
    // orderly-shutdown checks should apply (see Database::RazeAndPoison()).
    void Close(bool forced);

    // Construct a ScopedBlockingCall to annotate IO calls, but only if
    // database wasn't open in memory. ScopedBlockingCall uses |from_here| to
    // declare its blocking execution scope (see https://www.crbug/934302).
    void InitScopedBlockingCall(
        const base::Location& from_here,
        std::optional<base::ScopedBlockingCall>* scoped_blocking_call) const {
      if (database_)
        database_->InitScopedBlockingCall(from_here, scoped_blocking_call);
    }

   private:
    friend class base::RefCounted<StatementRef>;

    ~StatementRef();

    raw_ptr<Database> database_;
    raw_ptr<sqlite3_stmt> stmt_;
    bool was_valid_;
  };
  friend class StatementRef;

  // Executes a rollback statement, ignoring all transaction state. Used
  // internally in the transaction management code.
  void DoRollback();

  // Called by a StatementRef when it's being created or destroyed. See
  // open_statements_ below.
  void StatementRefCreated(StatementRef* ref);
  void StatementRefDeleted(StatementRef* ref);

  // Used by sql:: internals to report a SQLite error related to this database.
  //
  // `sqlite_error_code` contains the error code reported by SQLite. Possible
  // values are documented at https://www.sqlite.org/rescode.html
  //
  // `statement` is non-null if the error is associated with a sql::Statement.
  // Otherwise, `sql_statement` will be a non-null string pointing to a
  // statically-allocated (valid for the entire duration of the process) buffer
  // pointing to either a SQL statement or a SQL comment (starting with "-- ")
  // pointing to a "sqlite3_" function name.
  void OnSqliteError(SqliteErrorCode sqlite_error_code,
                     Statement* statement,
                     const char* sql_statement);

  // Like Execute(), but returns a SQLite result code.
  //
  // This method returns SqliteResultCode::kOk or a SQLite error code. In other
  // words, it never returns SqliteResultCode::{kDone, kRow}.
  //
  // This method is only exposed to the Database implementation. Code that uses
  // sql::Database should not be concerned with SQLite result codes.
  [[nodiscard]] SqliteResultCode ExecuteAndReturnResultCode(
      base::cstring_view sql);

  // Like |Execute()|, but retries if the database is locked.
  [[nodiscard]] bool ExecuteWithTimeout(base::cstring_view sql,
                                        base::TimeDelta ms_timeout);

  // Implementation helper for GetUniqueStatement() and GetCachedStatement().
  scoped_refptr<StatementRef> GetStatementImpl(base::cstring_view sql,
                                               bool is_readonly);

  // Release page-cache memory if memory-mapped I/O is enabled and the database
  // was changed.  Passing true for |implicit_change_performed| allows
  // overriding the change detection for cases like DDL (CREATE, DROP, etc),
  // which do not participate in the total-rows-changed tracking.
  void ReleaseCacheMemoryIfNeeded(bool implicit_change_performed);

  // Returns the results of sqlite3_db_filename(), which should match the path
  // passed to Open().
  base::FilePath DbPath() const;

  // Helper to collect diagnostic info for a corrupt database.
  std::string CollectCorruptionInfo();

  // Helper to collect diagnostic info for errors. `diagnostics` is an optional
  // out parameter. If `diagnostics` is defined, this method populates SOME of
  // its fields. Some of the fields are left unmodified for the caller.
  std::string CollectErrorInfo(int sqlite_error_code,
                               Statement* stmt,
                               DatabaseDiagnostics* diagnostics) const;

  // The size of the memory mapping that SQLite should use for this database.
  //
  // The return value follows the semantics of "PRAGMA mmap_size". In
  // particular, zero (0) means memory-mapping should be disabled, and the value
  // is capped by SQLITE_MAX_MMAP_SIZE. More details at
  // https://www.sqlite.org/pragma.html#pragma_mmap_size
  //
  // "Memory-mapped access" is usually shortened to "mmap", which is the name of
  // the POSIX system call used to implement. The same principles apply on
  // Windows, but its more-descriptive API names don't make for good shorthands.
  //
  // When mmap is enabled, SQLite attempts to use the memory-mapped area (by
  // calling xFetch() in the VFS file API) instead of requesting a database page
  // buffer from the pager and reading (via xRead() in the VFS API) into it.
  // When this works out, the database page cache ends up only storing pages
  // whose contents has been modified. More details at
  // https://sqlite.org/mmap.html
  //
  // I/O errors on memory-mapped files result in crashes in Chrome. POSIX
  // systems signal SIGSEGV or SIGBUS on I/O errors in mmap-ed files. Windows
  // raises the EXECUTE_IN_PAGE_ERROR strucuted exception in this case. Chrome
  // does not catch signals or structured exceptions.
  //
  // In order to avoid crashes, this method attempts to read the file using
  // regular I/O, and returns 0 (no mmap) if it encounters any error.
  size_t ComputeMmapSizeForOpen();

  // Helpers for ComputeMmapSizeForOpen().
  bool GetMmapAltStatus(int64_t* status);
  bool SetMmapAltStatus(int64_t status);

  // sqlite3_prepare_v3() flags for this database.
  int SqlitePrepareFlags() const;

  // Returns a SQLite VFS interface pointer to the file storing database pages.
  //
  // Returns null if the database is not backed by a VFS file. This is always
  // the case for in-memory databases.
  //
  // This method must only be called while the database is successfully opened.
  sqlite3_file* GetSqliteVfsFile();

  void SetEnableVirtualTablesForTesting(bool enable) {
    enable_virtual_tables_ = enable;
  }

  // Will eventually be checked on all methods. See https://crbug.com/1306694
  SEQUENCE_CHECKER(sequence_checker_);

  // The actual sqlite database. Will be null before Init has been called or if
  // Init resulted in an error.
  raw_ptr<sqlite3> db_ = nullptr;

  // TODO(shuagga@microsoft.com): Make `options_` const after removing all
  // setters.
  DatabaseOptions options_;

  // TODO(crbug.com/340805983): Remove this once virtual tables are no longer needed for
  // WebSQL, which requires them for fts3 support.
  bool enable_virtual_tables_ = false;

  // Holds references to all cached statements so they remain active.
  //
  // flat_map is appropriate here because the codebase has ~400 cached
  // statements, and each statement is at most one insertion in the map
  // throughout a process' lifetime.
  base::flat_map<StatementID, scoped_refptr<StatementRef>> statement_cache_;

  // A list of all StatementRefs we've given out. Each ref must register with
  // us when it's created or destroyed. This allows us to potentially close
  // any open statements when we encounter an error.
  std::set<raw_ptr<StatementRef>> open_statements_;

  // Number of currently-nested transactions.
  int transaction_nesting_ = 0;

  // True if any of the currently nested transactions have been rolled back.
  // When we get to the outermost transaction, this will determine if we do
  // a rollback instead of a commit.
  bool needs_rollback_ = false;

  // True if database is open with OpenInMemory(), False if database is open
  // with Open().
  bool in_memory_ = false;

  // |true| if the Database was closed using RazeAndPoison().  Used
  // to enable diagnostics to distinguish calls to never-opened
  // databases (incorrect use of the API) from calls to once-valid
  // databases.
  bool poisoned_ = false;

  // |true| if SQLite memory-mapped I/O is not desired for this database.
  bool mmap_disabled_;

  // |true| if SQLite memory-mapped I/O was enabled for this database.
  // Used by ReleaseCacheMemoryIfNeeded().
  bool mmap_enabled_ = false;

  // Used by ReleaseCacheMemoryIfNeeded() to track if new changes have happened
  // since memory was last released.
  int64_t total_changes_at_last_release_ = 0;

  // Called when a SQLite error occurs.
  //
  // This callback may be null, in which case errors are handled using a default
  // behavior.
  //
  // This callback must never be exposed outside this Database instance. This is
  // a straight-forward way to guarantee that this callback will not be called
  // after the Database instance goes out of scope. set_error_callback() makes
  // this guarantee.
  ErrorCallback error_callback_;

  // Developer-friendly database ID used in logging output and memory dumps.
  std::string histogram_tag_;

  // Stores the dump provider object when db is open.
  std::unique_ptr<DatabaseMemoryDumpProvider> memory_dump_provider_;

  // Vends WeakPtr<Database> for internal scoping helpers.
  base::WeakPtrFactory<Database> weak_factory_{this};
};

}  // namespace sql

#endif  // SQL_DATABASE_H_
