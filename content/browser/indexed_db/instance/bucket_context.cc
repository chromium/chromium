// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/bucket_context.h"

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/types/expected.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_internals_types.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "content/browser/indexed_db/blob_reader.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/leveldb/backing_store.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/status.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/origin.h"

namespace content::indexed_db {
namespace {

// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;

// This struct facilitates requesting bucket space usage from the quota manager.
// There have been reports of the callback being passed to the quota manager
// never being invoked. This struct will make sure to invoke the wrapped
// callback when it goes out of scope. The struct itself is in turn intended to
// be wrapped in a callback passed to the quota manager.
//
// There are three main tasks for this struct.
// * It makes sure the passed callback is run by doing so on destruction.
// * It logs UMA.
// * It times out the request if the quota manager is taking too long.
struct GetBucketSpaceRequestWrapper {
  static constexpr base::TimeDelta kTimeoutDuration = base::Seconds(45);
  // The timeout is split into 3 steps. See similar logic in
  // Transaction::kMaxTimeoutStrikes for reasoning.
  static constexpr int kTimeoutFraction = 3;

  explicit GetBucketSpaceRequestWrapper(
      base::OnceCallback<void(storage::QuotaErrorOr<int64_t>)> callback)
      : wrapped_callback(std::move(callback)) {
    StartTimer();
  }

  GetBucketSpaceRequestWrapper(GetBucketSpaceRequestWrapper&& other) {
    wrapped_callback = std::move(other.wrapped_callback);
    start_time = other.start_time;
    StartTimer();
  }

  ~GetBucketSpaceRequestWrapper() { InvokeCallback(); }

  void StartTimer() {
    timeout.Start(
        FROM_HERE,
        (timeouts_observed + 1) * (kTimeoutDuration / kTimeoutFraction) -
            (base::TimeTicks::Now() - start_time),
        base::BindOnce(&GetBucketSpaceRequestWrapper::TimeOut,
                       base::Unretained(this)));
  }

  void TimeOut() {
    if (++timeouts_observed == kTimeoutFraction) {
      InvokeCallback();
    } else {
      StartTimer();
    }
  }

  void InvokeCallback() {
    if (!wrapped_callback) {
      return;
    }

    static const char kDroppedRequest[] =
        "IndexedDB.QuotaCheckTime2.DroppedRequest";
    static const char kSuccess[] = "IndexedDB.QuotaCheckTime2.Success";
    static const char kQuotaError[] = "IndexedDB.QuotaCheckTime2.QuotaError";
    const char* histogram =
        result_value ? result_value->has_value() ? kSuccess : kQuotaError
                     : kDroppedRequest;
    base::UmaHistogramCustomTimes(
        histogram, base::TimeTicks::Now() - start_time, base::Milliseconds(1),
        kTimeoutDuration * 2, /*buckets=*/50U);

    std::move(wrapped_callback)
        .Run(result_value.value_or(
            base::unexpected(storage::QuotaError::kUnknownError)));
  }

  int timeouts_observed = 0;
  base::OneShotTimer timeout;
  base::OnceCallback<void(storage::QuotaErrorOr<int64_t>)> wrapped_callback;
  std::optional<storage::QuotaErrorOr<int64_t>> result_value;
  base::TimeTicks start_time = base::TimeTicks::Now();
};

DatabaseError CreateDefaultError() {
  return DatabaseError(
      blink::mojom::IDBException::kUnknownError,
      u"Internal error opening backing store for indexedDB.open.");
}

// Creates the leveldb and blob storage directories for IndexedDB.
std::
    tuple<base::FilePath /*leveldb_path*/, base::FilePath /*blob_path*/, Status>
    CreateDatabaseDirectories(const base::FilePath& path_base,
                              const storage::BucketLocator& bucket_locator) {
  Status status;
  if (!base::CreateDirectory(path_base)) {
    status = Status::IOError("Unable to create IndexedDB database path");
    LOG(ERROR) << status.ToString() << ": \"" << path_base.AsUTF8Unsafe()
               << "\"";
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                     bucket_locator);
    return {base::FilePath(), base::FilePath(), status};
  }

  base::FilePath leveldb_path =
      path_base.Append(GetLevelDBFileName(bucket_locator));
  base::FilePath blob_path =
      path_base.Append(GetBlobStoreFileName(bucket_locator));
  if (IsPathTooLong(leveldb_path)) {
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                     bucket_locator);
    status = Status::IOError("File path too long");
    return {base::FilePath(), base::FilePath(), status};
  }
  return {leveldb_path, blob_path, status};
}

}  // namespace

// TODO(crbug.com/40253999): Move to blink when needed there.
BASE_FEATURE(kSqliteBackingStore,
             "IdbSqliteBackingStore",
             base::FEATURE_DISABLED_BY_DEFAULT);

BucketContext::Delegate::Delegate()
    : on_ready_for_destruction(base::DoNothing()),
      on_receiver_bounced(base::DoNothing()),
      on_content_changed(base::DoNothing()),
      on_files_written(base::DoNothing()) {}

BucketContext::Delegate::Delegate(Delegate&& other) = default;
BucketContext::Delegate::~Delegate() = default;

BucketContext::BucketContext(
    storage::BucketInfo bucket_info,
    const base::FilePath& data_path,
    Delegate&& delegate,
    scoped_refptr<base::UpdateableSequencedTaskRunner> updateable_task_runner,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context)
    : bucket_info_(std::move(bucket_info)),
      updateable_task_runner_(updateable_task_runner),
      data_path_(data_path),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      blob_storage_context_(std::move(blob_storage_context)),
      file_system_access_context_(std::move(file_system_access_context)),
      delegate_(std::move(delegate)) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "BucketContext", base::SequencedTaskRunner::GetCurrentDefault(),
          base::trace_event::MemoryDumpProvider::Options());
  receivers_.set_disconnect_handler(base::BindRepeating(
      &BucketContext::OnReceiverDisconnected, base::Unretained(this)));
}

BucketContext::~BucketContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  delegate_.on_ready_for_destruction.Reset();
  ResetBackingStore();
}

void BucketContext::ForceClose(bool doom, const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_doomed_ = doom;

  {
    // This handle keeps `this` from closing until it goes out of scope.
    BucketContextHandle handle(*this);
    for (const auto& [name, database] : databases_) {
      // Note: We purposefully ignore the result here as force close needs to
      // continue tearing things down anyways.
      database->ForceCloseAndRunTasks(SanitizeErrorMessage(message));
    }
    databases_.clear();
    has_blobs_outstanding_ = false;
    close_timer_.Stop();
    if (backing_store()) {
      backing_store()->InvalidateBlobReferences();
      // Don't run the preclosing tasks after a ForceClose, whether or not we've
      // started them.  Compaction in particular can run long and cannot be
      // interrupted, so it can cause shutdown hangs.
      backing_store()->StopPreCloseTasks();
    }
    skip_closing_sequence_ = true;
  }

  // Initiate deletion if appropriate.
  RunTasks();
}

void BucketContext::StartMetadataRecording() {
  CHECK(!metadata_recording_enabled_);
  metadata_recording_start_time_ = base::Time::Now();
  metadata_recording_enabled_ = true;
  RecordInternalsSnapshot();  // Capture the initial snapshot.
}

std::vector<storage::mojom::IdbBucketMetadataPtr>
BucketContext::StopMetadataRecording() {
  metadata_recording_enabled_ = false;

  // Track the previous snapshot for each transaction in each database, keyed
  // by a string of db name, transaction ID and connection ID.
  std::unordered_map<std::string, storage::mojom::IdbTransactionMetadata*>
      transaction_snapshots;
  for (const storage::mojom::IdbBucketMetadataPtr& snapshot :
       metadata_recording_buffer_) {
    for (const storage::mojom::IdbDatabaseMetadataPtr& db :
         snapshot->databases) {
      for (const storage::mojom::IdbTransactionMetadataPtr& tx :
           db->transactions) {
        auto key = base::StringPrintf(
            "%s-%li-%i", base::UTF16ToASCII(db->name).c_str(),
            static_cast<long>(tx->tid), tx->connection_id);
        if (storage::mojom::IdbTransactionMetadata* prev_snapshot =
                base::FindPtrOrNull(transaction_snapshots, key)) {
          // Copy the state from the previous snapshot for this transaction ID.
          for (const auto& snap : prev_snapshot->state_history) {
            tx->state_history.push_back(snap->Clone());
          }
          tx->state_history.back()->duration += tx->age - prev_snapshot->age;
          if (prev_snapshot->state != tx->state) {
            tx->state_history.push_back(
                storage::mojom::IdbTransactionMetadataStateHistory::New(
                    tx->state, 0));
          }
        } else {
          tx->state_history.push_back(
              storage::mojom::IdbTransactionMetadataStateHistory::New(tx->state,
                                                                      tx->age));
        }
        transaction_snapshots[key] = tx.get();
      }
    }
  }

  return std::move(metadata_recording_buffer_);
}

int64_t BucketContext::GetInMemorySize() {
  return backing_store_ ? backing_store_->GetInMemorySize() : 0;
}

void BucketContext::ReportOutstandingBlobs(bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blobs_outstanding_ = blobs_outstanding;
  MaybeStartClosing();
}

void BucketContext::OnConnectionPriorityUpdated() {
  if (!updateable_task_runner_) {
    return;
  }
  base::TaskPriority priority = CalculateSchedulingPriority() == 0
                                    ? base::TaskPriority::USER_BLOCKING
                                    : base::TaskPriority::USER_VISIBLE;
  updateable_task_runner_->UpdatePriority(priority);
}

std::optional<int> BucketContext::CalculateSchedulingPriority() {
  std::optional<int> scheduling_priority;
  // Established connections:
  for (const auto& [name, database] : databases_) {
    for (auto* connection : database->connections()) {
      scheduling_priority = std::min(
          scheduling_priority.value_or(std::numeric_limits<int>::max()),
          connection->scheduling_priority());
    }
  }
  // Pending connections:
  for (auto iter = pending_connections_.begin();
       iter != pending_connections_.end();) {
    if (iter->WasInvalidated()) {
      iter = pending_connections_.erase(iter);
    } else {
      scheduling_priority = std::min(
          scheduling_priority.value_or(std::numeric_limits<int>::max()),
          (*iter)->scheduling_priority);
      ++iter;
    }
  }
  return scheduling_priority;
}

void BucketContext::CheckCanUseDiskSpace(
    int64_t space_requested,
    base::OnceCallback<void(bool)> bucket_space_check_callback) {
  if (space_requested <= GetBucketSpaceToAllot()) {
    if (bucket_space_check_callback) {
      bucket_space_remaining_ -= space_requested;
      std::move(bucket_space_check_callback).Run(/*allowed=*/true);
    }
    return;
  }

  bucket_space_remaining_ = 0;
  bucket_space_remaining_timestamp_ = base::TimeTicks();
  bool check_pending = !bucket_space_check_callbacks_.empty();
  bucket_space_check_callbacks_.emplace(space_requested,
                                        std::move(bucket_space_check_callback));
  if (!check_pending) {
    auto callback_with_logging = base::BindOnce(
        [](GetBucketSpaceRequestWrapper request_wrapper,
           storage::QuotaErrorOr<int64_t> result) {
          request_wrapper.result_value = result;
        },
        GetBucketSpaceRequestWrapper(
            base::BindOnce(&BucketContext::OnGotBucketSpaceRemaining,
                           weak_factory_.GetWeakPtr())));

    quota_manager()->GetBucketSpaceRemaining(
        bucket_locator(), base::SequencedTaskRunner::GetCurrentDefault(),
        std::move(callback_with_logging));
  }
}

void BucketContext::OnGotBucketSpaceRemaining(
    storage::QuotaErrorOr<int64_t> space_left) {
  bool allowed = space_left.has_value();
  bucket_space_remaining_ = space_left.value_or(0);
  bucket_space_remaining_timestamp_ = base::TimeTicks::Now();
  while (!bucket_space_check_callbacks_.empty()) {
    auto& [space_requested, result_callback] =
        bucket_space_check_callbacks_.front();
    allowed = allowed && (space_requested <= bucket_space_remaining_);

    if (allowed && result_callback) {
      bucket_space_remaining_ -= space_requested;
    }
    auto taken_callback = std::move(result_callback);
    bucket_space_check_callbacks_.pop();
    if (taken_callback) {
      std::move(taken_callback).Run(allowed);
    }
  }
}

int64_t BucketContext::GetBucketSpaceToAllot() {
  base::TimeDelta bucket_space_age =
      base::TimeTicks::Now() - bucket_space_remaining_timestamp_;
  if (bucket_space_age > kBucketSpaceCacheTimeLimit) {
    return 0;
  }
  return bucket_space_remaining_ *
         (1 - bucket_space_age / kBucketSpaceCacheTimeLimit);
}

void BucketContext::CreateAllExternalObjects(
    const std::vector<IndexedDBExternalObject>& objects,
    std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("IndexedDB", "BucketContext::CreateAllExternalObjects");

  DCHECK_EQ(objects.size(), mojo_objects->size());
  if (objects.empty()) {
    return;
  }

  for (size_t i = 0; i < objects.size(); ++i) {
    const IndexedDBExternalObject& blob_info = objects[i];
    blink::mojom::IDBExternalObjectPtr& mojo_object = (*mojo_objects)[i];

    switch (blob_info.object_type()) {
      case IndexedDBExternalObject::ObjectType::kBlob:
      case IndexedDBExternalObject::ObjectType::kFile: {
        DCHECK(mojo_object->is_blob_or_file());
        blink::mojom::IDBBlobInfoPtr& output_info =
            mojo_object->get_blob_or_file();

        mojo::PendingReceiver<blink::mojom::Blob> receiver =
            output_info->blob.InitWithNewPipeAndPassReceiver();
        if (blob_info.is_remote_valid()) {
          blob_info.Clone(std::move(receiver));
          continue;
        }

        BindBlobReader(blob_info, std::move(receiver));
        break;
      }
      case IndexedDBExternalObject::ObjectType::kFileSystemAccessHandle: {
        DCHECK(mojo_object->is_file_system_access_token());

        mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
            mojo_token;

        if (blob_info.is_file_system_access_remote_valid()) {
          blob_info.file_system_access_token_remote()->Clone(
              mojo_token.InitWithNewPipeAndPassReceiver());
        } else {
          DCHECK(!blob_info.serialized_file_system_access_handle().empty());
          file_system_access_context_->DeserializeHandle(
              bucket_info_.storage_key,
              blob_info.serialized_file_system_access_handle(),
              mojo_token.InitWithNewPipeAndPassReceiver());
        }
        mojo_object->get_file_system_access_token() = std::move(mojo_token);
        break;
      }
    }
  }
}

void BucketContext::QueueRunTasks() {
  if (task_run_queued_) {
    return;
  }

  task_run_queued_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BucketContext::RunTasks, weak_factory_.GetWeakPtr()));
}

void BucketContext::RunTasks() {
  task_run_queued_ = false;

  for (auto db_it = databases_.begin(); db_it != databases_.end();) {
    Database& db = *db_it->second;
    Status status = db.RunTasks();
    if (!status.ok()) {
      OnDatabaseError(status, {});
      return;
    }

    if (db.CanBeDestroyed()) {
      db_it = databases_.erase(db_it);
    } else {
      ++db_it;
    }
  }
  if (CanClose() && closing_stage_ == ClosingState::kClosed) {
    ResetBackingStore();
  }
}

void BucketContext::AddReceiver(
    const storage::BucketClientInfo& client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  // When `on_ready_for_destruction` is non-null, `this` hasn't requested its
  // own destruction. When it is null, this is to be torn down and has to bounce
  // the AddReceiver request back to the delegate.
  if (delegate().on_ready_for_destruction) {
    receivers_.Add(
        this, std::move(pending_receiver),
        ReceiverContext(client_info, std::move(client_state_checker_remote)));
  } else {
    delegate().on_receiver_bounced.Run(client_info,
                                       std::move(client_state_checker_remote),
                                       std::move(pending_receiver));
  }
}

void BucketContext::GetDatabaseInfo(GetDatabaseInfoCallback callback) {
  Status s;
  DatabaseError error;
  std::tie(s, error, std::ignore) =
      InitBackingStoreIfNeeded(/*create_if_missing=*/false);
  DCHECK_EQ(s.ok(), !!backing_store_);
  if (!s.ok()) {
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(error.code(), error.message()));

    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
    }
    return;
  }

  auto names_and_versions = backing_store_->GetDatabaseNamesAndVersions();
  if (!names_and_versions.has_value()) {
    std::move(callback).Run({}, blink::mojom::IDBError::New(
                                    blink::mojom::IDBException::kUnknownError,
                                    u"Internal error opening backing store for "
                                    "indexedDB.databases()."));
    return;
  }
  std::move(callback).Run(
      std::move(*names_and_versions),
      blink::mojom::IDBError::New(blink::mojom::IDBException::kNoError,
                                  std::u16string()));
}

void BucketContext::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        factory_client,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const std::u16string& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    int scheduling_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BucketContext::Open");
  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.

  bool was_cold_open = !backing_store_;
  Status s;
  DatabaseError error;
  IndexedDBDataLossInfo data_loss_info;
  std::tie(s, error, data_loss_info) =
      InitBackingStoreIfNeeded(/*create_if_missing=*/true);
  if (!backing_store_) {
    FactoryClient(std::move(factory_client)).OnError(error);
    if (s.IsCorruption()) {
      HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
    }
    return;
  }

  auto connection = std::make_unique<PendingConnection>(
      std::make_unique<FactoryClient>(std::move(factory_client)),
      std::make_unique<DatabaseCallbacks>(std::move(database_callbacks_remote)),
      transaction_id, version, std::move(transaction_receiver));
  connection->was_cold_open = was_cold_open;
  connection->data_loss_info = data_loss_info;
  connection->scheduling_priority = scheduling_priority;
  ReceiverContext& client = receivers_.current_context();
  // `Connection` only needs an opaque token to uniquely identify the
  // document or worker that owns the other side of the connection.
  connection->client_token = client.client_info.document_token
                                 ? client.client_info.document_token->value()
                                 : client.client_info.context_token.value();
  // Null in unit tests.
  if (client.client_state_checker_remote) {
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        state_checker_clone;
    client.client_state_checker_remote->MakeClone(
        state_checker_clone.InitWithNewPipeAndPassReceiver());
    connection->client_state_checker.Bind(std::move(state_checker_clone));
  }

  Database* database_ptr = nullptr;
  auto it = databases_.find(name);
  if (it == databases_.end()) {
    auto database = std::make_unique<Database>(name, *this);
    // The database must be added before the schedule call, as the
    // CreateDatabaseDeleteClosure can be called synchronously.
    database_ptr = database.get();
    AddDatabase(name, std::move(database));
  } else {
    database_ptr = it->second.get();

    // The `Database` might have been forced closed by dev tools, in which case
    // no new connections should be added. The `Database` should be deleted
    // *soon* in this case, but the request can arrive while `RunTasks()` is
    // still queued. We could try to reschedule this open() request, but if the
    // open request had already made it to ConnectionCoordinator, it would be
    // pruned and errors reported: see `ShouldPruneForForceClose()`. So do that
    // here too.
    if (!database_ptr->IsAcceptingConnections()) {
      connection->factory_client->OnError(
          DatabaseError(blink::mojom::IDBException::kAbortError,
                        "The connection was closed."));
      connection->database_callbacks->OnForcedClose();
      return;
    }
  }

  pending_connections_.push_back(connection->weak_factory.GetWeakPtr());
  database_ptr->ScheduleOpenConnection(std::move(connection));
  OnConnectionPriorityUpdated();
}

void BucketContext::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        factory_client,
    const std::u16string& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("IndexedDB", "BucketContext::DeleteDatabase");
  std::string force_close_message = "The database is deleted.";

  {
    Status s;
    DatabaseError error;
    // Note: Any data loss information here is not piped up to the renderer, and
    // will be lost.
    std::tie(s, error, std::ignore) = InitBackingStoreIfNeeded(
        /*create_if_missing=*/false);
    if (!backing_store_) {
      if (s.IsNotFound()) {
        FactoryClient(std::move(factory_client)).OnDeleteSuccess(/*version=*/0);
        return;
      }

      FactoryClient(std::move(factory_client)).OnError(error);
      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
      }
      return;
    }
  }
  auto on_deletion_complete =
      base::BindOnce(delegate().on_files_written, /*flushed=*/true);

  // First, check the databases that are already represented by
  // `Database` objects. If one exists, schedule it to be deleted and
  // we're done.
  auto it = databases_.find(name);
  if (it != databases_.end()) {
    base::WeakPtr<Database> database = it->second->AsWeakPtr();
    it->second->ScheduleDeleteDatabase(
        std::make_unique<FactoryClient>(std::move(factory_client)),
        std::move(on_deletion_complete));
    if (force_close) {
      Status status = database->ForceCloseAndRunTasks(force_close_message);
      if (!status.ok()) {
        OnDatabaseError(status, "Error aborting transactions.");
      }
    }
    return;
  }

  // Otherwise, verify that a database with the given name exists in the backing
  // store. If not, report success.
  base::expected<std::vector<std::u16string>, Status> names =
      backing_store()->GetDatabaseNames();
  if (!names.has_value()) {
    std::string error_message =
        "Internal error opening backing store for indexedDB.deleteDatabase.";
    DatabaseError error(blink::mojom::IDBException::kUnknownError,
                        error_message);
    FactoryClient(std::move(factory_client)).OnError(error);
    if (names.error().IsCorruption()) {
      HandleBackingStoreCorruption(error_message);
    }
    return;
  }

  if (!base::Contains(*names, name)) {
    FactoryClient(std::move(factory_client)).OnDeleteSuccess(/*version=*/0);
    return;
  }

  // If it exists but does not already have an `Database` object,
  // create it and initiate deletion.
  auto database = std::make_unique<Database>(name, *this);
  Database* database_ptr = AddDatabase(name, std::move(database));
  database_ptr->ScheduleDeleteDatabase(
      std::make_unique<FactoryClient>(std::move(factory_client)),
      std::move(on_deletion_complete));
  if (force_close) {
    Status status = database_ptr->ForceCloseAndRunTasks(force_close_message);
    if (!status.ok()) {
      OnDatabaseError(status, "Error aborting transactions.");
    }
  }
}

storage::mojom::IdbBucketMetadataPtr BucketContext::FillInMetadata(
    storage::mojom::IdbBucketMetadataPtr info) {
  // TODO(jsbell): Sort by name?
  std::vector<storage::mojom::IdbDatabaseMetadataPtr> database_list;
  if (backing_store_ && in_memory()) {
    info->size = GetInMemorySize();
  }
  for (const auto& [name, db] : databases_) {
    info->connection_count += db->ConnectionCount();
    database_list.push_back(db->GetIdbInternalsMetadata());
  }
  info->databases = std::move(database_list);
  for (const auto& [_, context] : receivers_.GetAllContexts()) {
    info->clients.push_back(context->client_info);
  }
  return info;
}

void BucketContext::NotifyOfIdbInternalsRelevantChange() {
  if (metadata_recording_enabled_) {
    RecordInternalsSnapshot();
  }
}

BucketContext* BucketContext::GetReferenceForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return this;
}

void BucketContext::FlushBackingStoreForTesting() {
  backing_store()->FlushForTesting();
}

void BucketContext::BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver) {
  level_db::BindMockFailureSingletonForTesting(std::move(receiver));  // IN-TEST
}

Database* BucketContext::AddDatabase(const std::u16string& name,
                                     std::unique_ptr<Database> database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(databases_, name));
  return databases_.emplace(name, std::move(database)).first->second.get();
}

void BucketContext::OnHandleCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++open_handles_;
  if (closing_stage_ != ClosingState::kNotClosing) {
    closing_stage_ = ClosingState::kNotClosing;
    close_timer_.Stop();
    if (backing_store()) {
      backing_store()->StopPreCloseTasks();
    }
  }
}

void BucketContext::OnHandleDestruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(open_handles_, 0ll);
  --open_handles_;
  MaybeStartClosing();
}

bool BucketContext::CanClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(open_handles_, 0);
  return !has_blobs_outstanding_ && open_handles_ <= 0 &&
         (!backing_store_ || is_doomed_ || !in_memory());
}

void BucketContext::MaybeStartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsClosing() && CanClose()) {
    StartClosing();
  }
}

void BucketContext::StartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanClose());
  DCHECK(!IsClosing());

  if (skip_closing_sequence_) {
    CloseNow();
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!close_timer_.IsRunning());
  closing_stage_ = ClosingState::kPreCloseGracePeriod;
  close_timer_.Start(FROM_HERE, base::Seconds(kBackingStoreGracePeriodSeconds),
                     base::BindOnce(&BucketContext::StartPreCloseTasks,
                                    weak_factory_.GetWeakPtr()));
}

void BucketContext::StartPreCloseTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (closing_stage_ != ClosingState::kPreCloseGracePeriod) {
    return;
  }
  closing_stage_ = ClosingState::kRunningPreCloseTasks;

  backing_store()->StartPreCloseTasks(base::BindOnce(
      [](base::WeakPtr<BucketContext> bucket_context) {
        if (!bucket_context || bucket_context->closing_stage_ !=
                                   ClosingState::kRunningPreCloseTasks) {
          return;
        }
        bucket_context->CloseNow();
      },
      weak_factory_.GetWeakPtr()));
}

void BucketContext::CloseNow() {
  closing_stage_ = ClosingState::kClosed;
  close_timer_.Stop();
  if (backing_store()) {
    backing_store()->StopPreCloseTasks();
  }
  QueueRunTasks();
}

void BucketContext::BindBlobReader(
    const IndexedDBExternalObject& blob_info,
    mojo::PendingReceiver<blink::mojom::Blob> blob_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::FilePath& path = blob_info.indexed_db_file_path();

  auto itr = file_reader_map_.find(path);
  if (itr == file_reader_map_.end()) {
    // Unretained is safe because `this` owns the reader.
    auto reader = std::make_unique<BlobReader>(
        blob_info, base::BindOnce(&BucketContext::RemoveBoundReaders,
                                  base::Unretained(this), path));
    itr =
        file_reader_map_
            .insert({path, std::make_tuple(std::move(reader),
                                           base::ScopedClosureRunner(
                                               blob_info.release_callback()))})
            .first;
  }

  std::get<0>(itr->second)
      ->AddReceiver(std::move(blob_receiver), *blob_storage_context_);
}

void BucketContext::RemoveBoundReaders(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_reader_map_.erase(path);
}

std::string BucketContext::SanitizeErrorMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = message;
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     data_path_.AsUTF8Unsafe(), "...");
  return sanitized_message;
}

bool BucketContext::ShouldUseSqliteBackingStore() {
  // Additional checks may be added subsequently.
  return base::FeatureList::IsEnabled(kSqliteBackingStore);
}

void BucketContext::HandleBackingStoreCorruption(
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string sanitized_error_message = SanitizeErrorMessage(error_message);
  base::OnceClosure handle_corruption =
      base::BindOnce(&level_db::BackingStore::HandleCorruption, data_path_,
                     bucket_locator(), sanitized_error_message);

  ForceClose(/*doom=*/false, sanitized_error_message);
  // In order to successfully delete the corrupted DB, the open handle must
  // first be closed.
  ResetBackingStore();
  // NOTE: `this` may be deleted (in tests, where `on_ready_for_destruction`
  // executes synchronously).

  std::move(handle_corruption).Run();
}

void BucketContext::OnDatabaseError(Status status, const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!status.ok());
  const std::string error_message =
      message.empty() ? status.ToString() : message;
  if (status.IsCorruption()) {
    HandleBackingStoreCorruption(error_message);
    return;
  }
  if (status.IsIOError()) {
    quota_manager_proxy_->OnClientWriteFailed(bucket_info_.storage_key);
  }
  ForceClose(/*doom=*/false, error_message);
}

bool BucketContext::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backing_store_) {
    // Nothing to report when no databases have been loaded.
    return true;
  }

  base::CheckedNumeric<uint64_t> total_memory_in_flight = 0;
  for (const auto& [name, database] : databases_) {
    for (Connection* connection : database->connections()) {
      for (const auto& txn_id_pair : connection->transactions()) {
        total_memory_in_flight += txn_id_pair.second->in_flight_memory();
      }
    }
  }
  auto* db_dump = pmd->CreateAllocatorDump(
      base::StringPrintf("site_storage/index_db/in_flight_0x%" PRIXPTR,
                         backing_store()->GetIdentifierForMemoryDump()));
  db_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     total_memory_in_flight.ValueOrDefault(0));
  return true;
}

std::tuple<Status, DatabaseError, IndexedDBDataLossInfo>
BucketContext::InitBackingStoreIfNeeded(bool create_if_missing) {
  if (backing_store_) {
    return {};
  }

  base::FilePath blob_path;
  base::FilePath database_path;
  Status status = Status::OK();
  if (!in_memory()) {
    std::tie(database_path, blob_path, status) =
        CreateDatabaseDirectories(data_path_, bucket_locator());
    if (!status.ok()) {
      return {status, CreateDefaultError(), IndexedDBDataLossInfo()};
    }
  }

  auto lock_manager = std::make_unique<PartitionedLockManager>();
  IndexedDBDataLossInfo data_loss_info;
  std::unique_ptr<BackingStore> backing_store;
  bool disk_full = false;
  base::ElapsedTimer open_timer;
  Status first_try_status;
  constexpr static const int kNumOpenTries = 2;
  for (int i = 0; i < kNumOpenTries; ++i) {
    const bool is_first_attempt = i == 0;
    std::tie(backing_store, status, data_loss_info, disk_full) =
        ShouldUseSqliteBackingStore()
            ? sqlite::BackingStoreImpl::OpenAndVerify(data_path_)
            : level_db::BackingStore::OpenAndVerify(
                  *this, data_path_, database_path, blob_path,
                  lock_manager.get(), is_first_attempt, create_if_missing);
    if (is_first_attempt) [[likely]] {
      first_try_status = status;
    }
    if (status.ok()) [[likely]] {
      break;
    }
    if (!create_if_missing && status.IsNotFound()) {
      return {status, DatabaseError(), data_loss_info};
    }
    DCHECK(!backing_store);
    // If the disk is full, always exit immediately.
    if (disk_full) {
      break;
    }
  }

  // Record this here because the !create_if_missing && not_found case shouldn't
  // count as either a success or failure.
  base::UmaHistogramEnumeration(kBackingStoreActionUmaName,
                                IndexedDBAction::kBackingStoreOpenAttempt);

  first_try_status.Log("WebCore.IndexedDB.BackingStore.OpenFirstTryResult");

  if (first_try_status.ok()) [[likely]] {
    UMA_HISTOGRAM_TIMES(
        "WebCore.IndexedDB.BackingStore.OpenFirstTrySuccessTime",
        open_timer.Elapsed());
  }

  if (status.ok()) [[likely]] {
    base::UmaHistogramTimes("WebCore.IndexedDB.BackingStore.OpenSuccessTime",
                            open_timer.Elapsed());
  } else {
    base::UmaHistogramTimes("WebCore.IndexedDB.BackingStore.OpenFailureTime",
                            open_timer.Elapsed());
    if (disk_full) {
      ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_DISK_FULL,
                       bucket_locator());
      quota_manager()->OnClientWriteFailed(bucket_locator().storage_key);
      return {status,
              DatabaseError(blink::mojom::IDBException::kQuotaError,
                            u"Encountered full disk while opening "
                            "backing store for indexedDB.open."),
              data_loss_info};
    }
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
                     bucket_locator());
    return {status, CreateDefaultError(), data_loss_info};
  }

  if (!in_memory()) {
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_SUCCESS, bucket_locator());
  }

  lock_manager_ = std::move(lock_manager);
  backing_store_ = std::move(backing_store);
  delegate().on_files_written.Run(/*flushed=*/true);
  return {Status::OK(), DatabaseError(), data_loss_info};
}

void BucketContext::ResetBackingStore() {
  file_reader_map_.clear();
  weak_factory_.InvalidateWeakPtrs();

  if (backing_store_) {
    base::WaitableEvent leveldb_destruct_event;
    backing_store_->TearDown(&leveldb_destruct_event);
    const auto start = base::TimeTicks::Now();
    backing_store_.reset();
    leveldb_destruct_event.Wait();
    base::UmaHistogramTimes("IndexedDB.BackingStoreCloseDuration",
                            base::TimeTicks::Now() - start);
  }

  task_run_queued_ = false;
  is_doomed_ = false;
  bucket_space_check_callbacks_ = {};
  open_handles_ = 0;
  databases_.clear();
  lock_manager_.reset();
  close_timer_.Stop();
  closing_stage_ = ClosingState::kNotClosing;
  has_blobs_outstanding_ = false;
  skip_closing_sequence_ = false;
  running_tasks_ = false;

  if (receivers_.empty() && delegate().on_ready_for_destruction) {
    std::move(delegate().on_ready_for_destruction).Run();
  }
}

void BucketContext::OnReceiverDisconnected() {
  if (receivers_.empty() && !backing_store_ &&
      delegate().on_ready_for_destruction) {
    std::move(delegate().on_ready_for_destruction).Run();
  }
}

void BucketContext::RecordInternalsSnapshot() {
  storage::mojom::IdbBucketMetadataPtr metadata =
      storage::mojom::IdbBucketMetadata::New();
  metadata->bucket_locator = bucket_locator();
  metadata = FillInMetadata(std::move(metadata));
  metadata->delta_recording_start_ms =
      (base::Time::Now() - metadata_recording_start_time_).InMilliseconds();
  metadata_recording_buffer_.push_back(std::move(metadata));
}

BucketContext::ReceiverContext::ReceiverContext(
    const storage::BucketClientInfo& client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote)
    : client_info(client_info),
      client_state_checker_remote(std::move(client_state_checker_remote)) {}

BucketContext::ReceiverContext::ReceiverContext(
    BucketContext::ReceiverContext&&) noexcept = default;
BucketContext::ReceiverContext::~ReceiverContext() = default;

}  // namespace content::indexed_db
