// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_channel_id_store.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "crypto/ec_private_key.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/cookie_util.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 6;
const int kCompatibleVersionNumber = 6;

// Used in the DomainBoundCerts.DBLoadStatus histogram to record the status of
// the Channel ID database when loading it from disk. It reports reasons why the
// db could fail to load, or that it was loaded successfully.
// Do not change or re-use values.
enum DbLoadStatus {
  // The path for the directory containing the db doesn't exist and couldn't be
  // created.
  PATH_DOES_NOT_EXIST = 0,
  // Unable to open the database.
  FAILED_TO_OPEN = 1,
  // Failed to migrate the db to the current version.
  MIGRATION_FAILED = 2,
  // Unable to execute SELECT statement to load contents from db.
  INVALID_SELECT_STATEMENT = 3,
  // New database successfully created.
  NEW_DB = 4,
  // Database successfully loaded.
  LOADED = 5,
  // Database loaded, but one or more keys were skipped.
  LOADED_WITH_ERRORS = 6,
  DB_LOAD_STATUS_MAX
};

void RecordDbLoadStatus(DbLoadStatus status) {
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.DBLoadStatus", status,
                            DB_LOAD_STATUS_MAX);
}

}  // namespace

namespace net {

// This class is designed to be shared between any calling threads and the
// background task runner. It batches operations and commits them on a timer.
class SQLiteChannelIDStore::Backend
    : public base::RefCountedThreadSafe<SQLiteChannelIDStore::Backend> {
 public:
  Backend(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : path_(path),
        num_pending_(0),
        force_keep_session_state_(false),
        background_task_runner_(background_task_runner),
        corruption_detected_(false) {}

  // Creates or loads the SQLite database.
  void Load(const LoadedCallback& loaded_callback);

  // Batch a channel ID addition.
  void AddChannelID(const DefaultChannelIDStore::ChannelID& channel_id);

  // Batch a channel ID deletion.
  void DeleteChannelID(const DefaultChannelIDStore::ChannelID& channel_id);

  // Post background delete of all channel ids for |server_identifiers|.
  void DeleteAllInList(const std::list<std::string>& server_identifiers);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

  void SetForceKeepSessionState();

  // Posts a task to flush pending operations to the database.
  void Flush();

 private:
  friend class base::RefCountedThreadSafe<SQLiteChannelIDStore::Backend>;

  // You should call Close() before destructing this object.
  virtual ~Backend() {
    DCHECK(!db_.get()) << "Close should have already been called.";
    DCHECK_EQ(0u, num_pending_);
    DCHECK(pending_.empty());
  }

  void LoadInBackground(
      std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>*
          channel_ids);

  // Database upgrade statements.
  bool EnsureDatabaseVersion();

  class PendingOperation {
   public:
    enum OperationType { CHANNEL_ID_ADD, CHANNEL_ID_DELETE };

    PendingOperation(OperationType op,
                     const DefaultChannelIDStore::ChannelID& channel_id)
        : op_(op), channel_id_(channel_id) {}

    OperationType op() const { return op_; }
    const DefaultChannelIDStore::ChannelID& channel_id() const {
      return channel_id_;
    }

   private:
    OperationType op_;
    DefaultChannelIDStore::ChannelID channel_id_;
  };

 private:
  // Batch a channel id operation (add or delete).
  void BatchOperation(PendingOperation::OperationType op,
                      const DefaultChannelIDStore::ChannelID& channel_id);
  // Prunes the list of pending operations to remove any operations for an
  // identifier in |server_identifiers|.
  void PrunePendingOperationsForDeletes(
      const std::list<std::string>& server_identifiers);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background task runner.
  void InternalBackgroundClose();

  void BackgroundDeleteAllInList(
      const std::list<std::string>& server_identifiers);

  void DatabaseErrorCallback(int error, sql::Statement* stmt);
  void KillDatabase();

  const base::FilePath path_;
  std::unique_ptr<sql::Database> db_;
  sql::MetaTable meta_table_;

  typedef std::list<std::unique_ptr<PendingOperation>> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  // True if the persistent store should skip clear on exit rules.
  bool force_keep_session_state_;
  // Guard |pending_|, |num_pending_| and |force_keep_session_state_|.
  base::Lock lock_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Indicates if the kill-database callback has been scheduled.
  bool corruption_detected_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

void SQLiteChannelIDStore::Backend::Load(
    const LoadedCallback& loaded_callback) {
  // This function should be called only once per instance.
  DCHECK(!db_.get());
  std::unique_ptr<
      std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>>
      channel_ids(
          new std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>());
  std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>*
      channel_ids_ptr = channel_ids.get();

  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&Backend::LoadInBackground, this, channel_ids_ptr),
      base::Bind(loaded_callback, base::Passed(&channel_ids)));
}

void SQLiteChannelIDStore::Backend::LoadInBackground(
    std::vector<std::unique_ptr<DefaultChannelIDStore::ChannelID>>*
        channel_ids) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  // This method should be called only once per instance.
  DCHECK(!db_.get());

  // Ensure the parent directory for storing certs is created before reading
  // from it.
  const base::FilePath dir = path_.DirName();
  if (!base::PathExists(dir) && !base::CreateDirectory(dir)) {
    RecordDbLoadStatus(PATH_DOES_NOT_EXIST);
    return;
  }

  db_.reset(new sql::Database);
  db_->set_histogram_tag("DomainBoundCerts");

  // Unretained to avoid a ref loop with db_.
  db_->set_error_callback(
      base::Bind(&SQLiteChannelIDStore::Backend::DatabaseErrorCallback,
                 base::Unretained(this)));

  DbLoadStatus load_result = LOADED;
  if (!base::PathExists(path_)) {
    load_result = NEW_DB;
  }

  if (!db_->Open(path_)) {
    NOTREACHED() << "Unable to open cert DB.";
    if (corruption_detected_)
      KillDatabase();
    db_.reset();
    RecordDbLoadStatus(FAILED_TO_OPEN);
    return;
  }

  if (!EnsureDatabaseVersion()) {
    NOTREACHED() << "Unable to open cert DB.";
    if (corruption_detected_)
      KillDatabase();
    meta_table_.Reset();
    db_.reset();
    RecordDbLoadStatus(MIGRATION_FAILED);
    return;
  }

  db_->Preload();

  // Slurp all the certs into the out-vector.
  sql::Statement smt(db_->GetUniqueStatement(
      "SELECT host, private_key, creation_time FROM channel_id"));
  if (!smt.is_valid()) {
    if (corruption_detected_)
      KillDatabase();
    meta_table_.Reset();
    db_.reset();
    RecordDbLoadStatus(INVALID_SELECT_STATEMENT);
    return;
  }

  while (smt.Step()) {
    std::vector<uint8_t> private_key_from_db;
    smt.ColumnBlobAsVector(1, &private_key_from_db);
    std::unique_ptr<crypto::ECPrivateKey> key(
        crypto::ECPrivateKey::CreateFromPrivateKeyInfo(private_key_from_db));
    if (!key) {
      load_result = LOADED_WITH_ERRORS;
      continue;
    }
    std::unique_ptr<DefaultChannelIDStore::ChannelID> channel_id(
        new DefaultChannelIDStore::ChannelID(
            smt.ColumnString(0),  // host
            base::Time::FromInternalValue(smt.ColumnInt64(2)), std::move(key)));
    channel_ids->push_back(std::move(channel_id));
  }

  RecordDbLoadStatus(load_result);
}

bool SQLiteChannelIDStore::Backend::EnsureDatabaseVersion() {
  // Version check.
  if (!meta_table_.Init(
          db_.get(), kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Server bound cert database is too new.";
    return false;
  }

  int cur_version = meta_table_.GetVersionNumber();
  UMA_HISTOGRAM_EXACT_LINEAR("DomainBoundCerts.DBVersion", cur_version,
                             kCurrentVersionNumber + 1);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // Create new table if it doesn't already exist
  if (!db_->DoesTableExist("channel_id")) {
    if (!db_->Execute(
            "CREATE TABLE channel_id ("
            "host TEXT NOT NULL UNIQUE PRIMARY KEY,"
            "private_key BLOB NOT NULL,"
            "public_key BLOB NOT NULL,"
            "creation_time INTEGER)")) {
      return false;
    }
  }

  if (cur_version < kCurrentVersionNumber) {
    if (cur_version <= 4) {
      sql::Statement statement(
          db_->GetUniqueStatement("DROP TABLE origin_bound_certs"));
      if (!statement.Run()) {
        LOG(WARNING) << "Error dropping old origin_bound_certs table";
        return false;
      }
    }
    if (cur_version < 6) {
      // The old format had the private_key column in a format we no longer
      // read. Just delete any entries in that format.
      sql::Statement statement(
          db_->GetUniqueStatement("DELETE FROM channel_id"));
      if (!statement.Run()) {
        LOG(WARNING) << "Error clearing channel_id table";
        return false;
      }
    }
    meta_table_.SetVersionNumber(kCurrentVersionNumber);
    meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber);
  }
  transaction.Commit();

  // Put future migration cases here.

  return true;
}

void SQLiteChannelIDStore::Backend::DatabaseErrorCallback(
    int error,
    sql::Statement* stmt) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!sql::IsErrorCatastrophic(error))
    return;

  // TODO(shess): Running KillDatabase() multiple times should be
  // safe.
  if (corruption_detected_)
    return;

  corruption_detected_ = true;

  // TODO(shess): Consider just calling RazeAndClose() immediately.
  // db_ may not be safe to reset at this point, but RazeAndClose()
  // would cause the stack to unwind safely with errors.
  background_task_runner_->PostTask(FROM_HERE,
                                    base::Bind(&Backend::KillDatabase, this));
}

void SQLiteChannelIDStore::Backend::KillDatabase() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (db_) {
    // This Backend will now be in-memory only. In a future run the database
    // will be recreated. Hopefully things go better then!
    bool success = db_->RazeAndClose();
    UMA_HISTOGRAM_BOOLEAN("DomainBoundCerts.KillDatabaseResult", success);
    meta_table_.Reset();
    db_.reset();
  }
}

void SQLiteChannelIDStore::Backend::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  BatchOperation(PendingOperation::CHANNEL_ID_ADD, channel_id);
}

void SQLiteChannelIDStore::Backend::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  BatchOperation(PendingOperation::CHANNEL_ID_DELETE, channel_id);
}

void SQLiteChannelIDStore::Backend::DeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  if (server_identifiers.empty())
    return;
  // Perform deletion on background task runner.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &Backend::BackgroundDeleteAllInList, this, server_identifiers));
}

void SQLiteChannelIDStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const DefaultChannelIDStore::ChannelID& channel_id) {
  // These thresholds used to be 30 seconds or 512 outstanding operations (the
  // same values used in CookieMonster). Since cookies can be bound to Channel
  // IDs, it's possible for a cookie to get committed to the cookie database
  // before the Channel ID it is bound to gets committed. Decreasing these
  // thresholds increases the chance that the Channel ID will be committed
  // before or at the same time as the cookie.

  // Commit every 2 seconds.
  static const int kCommitIntervalMs = 2 * 1000;
  // Commit right away if we have more than 3 outstanding operations.
  static const size_t kCommitAfterBatchSize = 3;

  // We do a full copy of the cert here, and hopefully just here.
  std::unique_ptr<PendingOperation> po(new PendingOperation(op, channel_id));

  PendingOperationsList::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    pending_.push_back(std::move(po));
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Backend::Commit, this),
        base::TimeDelta::FromMilliseconds(kCommitIntervalMs));
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    background_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(&Backend::Commit, this));
  }
}

void SQLiteChannelIDStore::Backend::PrunePendingOperationsForDeletes(
    const std::list<std::string>& server_identifiers) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock locked(lock_);

  for (auto it = pending_.begin(); it != pending_.end();) {
    if (base::ContainsValue(server_identifiers,
                            (*it)->channel_id().server_identifier())) {
      std::unique_ptr<PendingOperation> po(std::move(*it));
      it = pending_.erase(it);
      --num_pending_;
    } else {
      ++it;
    }
  }
}

void SQLiteChannelIDStore::Backend::Flush() {
  if (background_task_runner_->RunsTasksInCurrentSequence()) {
    Commit();
  } else {
    background_task_runner_->PostTask(FROM_HERE,
                                      base::Bind(&Backend::Commit, this));
  }
}

void SQLiteChannelIDStore::Backend::Commit() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  PendingOperationsList ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_.get() || ops.empty())
    return;

  sql::Statement add_statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO channel_id (host, private_key, public_key, creation_time) "
      "VALUES (?,?,\"\",?)"));
  if (!add_statement.is_valid())
    return;

  sql::Statement del_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM channel_id WHERE host=?"));
  if (!del_statement.is_valid())
    return;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (auto it = ops.begin(); it != ops.end(); ++it) {
    // Free the certs as we commit them to the database.
    std::unique_ptr<PendingOperation> po(std::move(*it));
    switch (po->op()) {
      case PendingOperation::CHANNEL_ID_ADD: {
        add_statement.Reset(true);
        add_statement.BindString(0, po->channel_id().server_identifier());
        std::vector<uint8_t> private_key;
        if (!po->channel_id().key()->ExportPrivateKey(&private_key))
          continue;
        add_statement.BindBlob(
            1, private_key.data(), static_cast<int>(private_key.size()));
        add_statement.BindInt64(
            2, po->channel_id().creation_time().ToInternalValue());
        if (!add_statement.Run())
          NOTREACHED() << "Could not add a server bound cert to the DB.";
        break;
      }
      case PendingOperation::CHANNEL_ID_DELETE:
        del_statement.Reset(true);
        del_statement.BindString(0, po->channel_id().server_identifier());
        if (!del_statement.Run())
          NOTREACHED() << "Could not delete a server bound cert from the DB.";
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  transaction.Commit();
}

// Fire off a close message to the background task runner. We could still have a
// pending commit timer that will be holding a reference on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void SQLiteChannelIDStore::Backend::Close() {
  // Must close the backend on the background task runner.
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Backend::InternalBackgroundClose, this));
}

void SQLiteChannelIDStore::Backend::InternalBackgroundClose() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  // Commit any pending operations
  Commit();
  db_.reset();
}

void SQLiteChannelIDStore::Backend::BackgroundDeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  if (!db_.get())
    return;

  PrunePendingOperationsForDeletes(server_identifiers);

  sql::Statement del_smt(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM channel_id WHERE host=?"));
  if (!del_smt.is_valid()) {
    LOG(WARNING) << "Unable to delete channel ids.";
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(WARNING) << "Unable to delete channel ids.";
    return;
  }

  for (auto it = server_identifiers.begin(); it != server_identifiers.end();
       ++it) {
    del_smt.Reset(true);
    del_smt.BindString(0, *it);
    if (!del_smt.Run())
      NOTREACHED() << "Could not delete a channel id from the DB.";
  }

  if (!transaction.Commit())
    LOG(WARNING) << "Unable to delete channel ids.";
}

void SQLiteChannelIDStore::Backend::SetForceKeepSessionState() {
  base::AutoLock locked(lock_);
  force_keep_session_state_ = true;
}

SQLiteChannelIDStore::SQLiteChannelIDStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : backend_(new Backend(path, background_task_runner)) {
}

void SQLiteChannelIDStore::Load(const LoadedCallback& loaded_callback) {
  backend_->Load(loaded_callback);
}

void SQLiteChannelIDStore::AddChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  backend_->AddChannelID(channel_id);
}

void SQLiteChannelIDStore::DeleteChannelID(
    const DefaultChannelIDStore::ChannelID& channel_id) {
  backend_->DeleteChannelID(channel_id);
}

void SQLiteChannelIDStore::DeleteAllInList(
    const std::list<std::string>& server_identifiers) {
  backend_->DeleteAllInList(server_identifiers);
}

void SQLiteChannelIDStore::SetForceKeepSessionState() {
  backend_->SetForceKeepSessionState();
}

void SQLiteChannelIDStore::Flush() {
  backend_->Flush();
}

SQLiteChannelIDStore::~SQLiteChannelIDStore() {
  backend_->Close();
  // We release our reference to the Backend, though it will probably still have
  // a reference if the background task runner has not run Close() yet.
}

}  // namespace net
