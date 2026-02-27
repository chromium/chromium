// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/bucket_context.h"

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <functional>
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

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
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
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/instance/blob_reader.h"
#include "content/browser/indexed_db/instance/bucket_context_handle.h"
#include "content/browser/indexed_db/instance/connection.h"
#include "content/browser/indexed_db/instance/database.h"
#include "content/browser/indexed_db/instance/database_callbacks.h"
#include "content/browser/indexed_db/instance/leveldb/backing_store.h"
#include "content/browser/indexed_db/instance/pending_connection.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"
#include "content/browser/indexed_db/status.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace content::indexed_db {
namespace {

// This flag enables the SQLite backing store for in-memory contexts.
BASE_FEATURE(kIdbSqliteBackingStoreInMemoryContexts,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIdbSqliteOnDiskRollout, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<SqliteRolloutStage>::Option
    kIdbSqliteOnDiskRolloutStages[] = {
        {SqliteRolloutStage::kUseLevelDbOnly, "UseLevelDbOnly"},
        {SqliteRolloutStage::kUseSqliteForNewStores, "UseSqliteForNewStores"},
        {SqliteRolloutStage::kUseSqliteOnly, "UseSqliteOnly"},
};

BASE_FEATURE_ENUM_PARAM(SqliteRolloutStage,
                        kIdbSqliteOnDiskRolloutStage,
                        &kIdbSqliteOnDiskRollout,
                        "stage",
                        SqliteRolloutStage::kUseLevelDbOnly,
                        &kIdbSqliteOnDiskRolloutStages);

// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;

// Duration of inactivity after which idle tasks are run.
constexpr base::TimeDelta kIdleTimeout = base::Seconds(5);

std::optional<bool> g_should_use_sqlite_for_testing;

base::OnceClosure& GetTeardownExtraStepForTesting() {
  static base::NoDestructor<base::OnceClosure> g_teardown_override_for_testing;
  return *g_teardown_override_for_testing;
}

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

// Returns the SQLite rollout stage by taking into account all global test
// overrides and feature states.
SqliteRolloutStage GetSqliteRolloutStage(bool in_memory) {
  if (g_should_use_sqlite_for_testing.has_value()) {
    return *g_should_use_sqlite_for_testing
               ? SqliteRolloutStage::kUseSqliteOnly
               : SqliteRolloutStage::kUseLevelDbOnly;
  }
  if (base::FeatureList::IsEnabled(features::kIdbSqliteBackingStore)) {
    return SqliteRolloutStage::kUseSqliteOnly;
  }
  if (in_memory) {
    return base::FeatureList::IsEnabled(kIdbSqliteBackingStoreInMemoryContexts)
               ? SqliteRolloutStage::kUseSqliteOnly
               : SqliteRolloutStage::kUseLevelDbOnly;
  }
  if (base::FeatureList::IsEnabled(kIdbSqliteOnDiskRollout)) {
    return kIdbSqliteOnDiskRolloutStage.Get();
  }
  return SqliteRolloutStage::kUseLevelDbOnly;
}

bool ShouldUseSqlite(SqliteRolloutStage stage,
                     const storage::BucketLocator& bucket_locator,
                     const base::FilePath& data_path) {
  switch (stage) {
    case SqliteRolloutStage::kUseLevelDbOnly:
      return false;
    case SqliteRolloutStage::kUseSqliteForNewStores:
      CHECK(!data_path.empty());
      return !base::PathExists(
          data_path.Append(GetLevelDBFileName(bucket_locator)));
    case SqliteRolloutStage::kUseSqliteOnly:
      return true;
  }
}

}  // namespace

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
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context)
    : bucket_info_(std::move(bucket_info)),
      data_path_(data_path),
      idle_timer_(FROM_HERE,
                  kIdleTimeout,
                  base::BindRepeating(&BucketContext::RunIdleTasks,
                                      base::Unretained(this))),
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
  sqlite_rollout_stage_ = GetSqliteRolloutStage(in_memory());
}

BucketContext::~BucketContext() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  delegate_.on_ready_for_destruction.Reset();
  ResetBackingStore();

  if (delegate_.on_destroyed) {
    std::move(delegate_.on_destroyed).Run();
  }
}

// static
uint64_t BucketContext::ReadUsageFromDisk(
    const storage::BucketLocator& bucket_locator,
    const base::FilePath& data_path) {
  CHECK(!data_path.empty());
  return ShouldUseSqlite(GetSqliteRolloutStage(/*in_memory=*/false),
                         bucket_locator, data_path)
             ? sqlite::BackingStoreImpl::SumSizesOfDatabaseFiles(
                   data_path.Append(GetSqliteDbDirectory(bucket_locator)))
             : level_db::BackingStore::ReadSizeFromDisk(
                   data_path.Append(GetLevelDBFileName(bucket_locator)),
                   data_path.Append(GetBlobStoreFileName(bucket_locator)));
}

void BucketContext::ForceClose(bool doom, const std::string& message) {
  is_doomed_ = doom;

  {
    // This handle keeps `this` from closing until it goes out of scope.
    BucketContextHandle handle(*this);
    if (backing_store()) {
      backing_store()->OnForceClosing();
    }
    for (auto& [_, db] : databases_) {
      db->ForceCloseConnectionsAndCancelRequests(SanitizeErrorMessage(message));
      CHECK(db->CanBeDestroyed());
    }
    databases_.clear();
    has_blobs_outstanding_ = false;
    close_timer_.Stop();
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

uint64_t BucketContext::GetUsage(bool write_in_progress) {
  return backing_store_ ? backing_store()->EstimateSize(write_in_progress)
         : in_memory()  ? 0
                        : ReadUsageFromDisk(bucket_locator(), data_path_);
}

void BucketContext::ReportOutstandingBlobs(bool blobs_outstanding) {
  has_blobs_outstanding_ = blobs_outstanding;
  MaybeStartClosing();
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
  CHECK(!IsUsingSqlite());

  TRACE_EVENT0("IndexedDB", "BucketContext::CreateAllExternalObjects");

  CHECK_EQ(objects.size(), mojo_objects->size());
  if (objects.empty()) {
    return;
  }

  for (size_t i = 0; i < objects.size(); ++i) {
    const IndexedDBExternalObject& blob_info = objects[i];
    blink::mojom::IDBExternalObjectPtr& mojo_object = (*mojo_objects)[i];

    switch (blob_info.object_type()) {
      case IndexedDBExternalObject::ObjectType::kBlob:
      case IndexedDBExternalObject::ObjectType::kFile: {
        CHECK(mojo_object->is_blob_or_file());
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
        CHECK(mojo_object->is_file_system_access_token());

        mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
            mojo_token;

        if (blob_info.is_file_system_access_remote_valid()) {
          blob_info.file_system_access_token_remote()->Clone(
              mojo_token.InitWithNewPipeAndPassReceiver());
        } else {
          CHECK(!blob_info.serialized_file_system_access_handle().empty());
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

bool BucketContext::IsUsingSqlite() {
  CHECK(backing_store_);
  return std::get<bool>(*backing_store_);
}

void BucketContext::QueueRunTasks() {
  TRACE_EVENT0("IndexedDB", "BucketContext::QueueRunTasks");

  if (task_run_queued_) {
    TRACE_EVENT_INSTANT("IndexedDB",
                        "BucketContext::QueueRunTasks - Already queued");
    return;
  }

  task_run_queued_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BucketContext::RunTasks, weak_factory_.GetWeakPtr()));
}

void BucketContext::RunTasks() {
  task_run_queued_ = false;
  if (last_idle_tasks_completion_time_) {
    base::UmaHistogramMediumTimes(
        base::StrCat({"IndexedDB.IdleTasksCompletionToNextActivity",
                      ToVariantSuffix(in_memory())}),
        base::TimeTicks::Now() - *last_idle_tasks_completion_time_);
    last_idle_tasks_completion_time_.reset();
  }

  for (auto db_it = databases_.begin(); db_it != databases_.end();) {
    Database& db = *db_it->second;
    Status status = db.RunTasks();
    if (!status.ok()) {
      OnDatabaseError(&db, status, {});
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
  } else {
    // Run idle tasks after a delay if there are no more immediate tasks to run.
    idle_timer_.Reset();
    if (IsUsingSqlite()) {
      // Since a `Database` may have just been destroyed, there may no longer be
      // a need to keep `this` around. Note that this isn't necessary in LevelDB
      // due to differences in `CanClose()`, although it likely wouldn't be
      // harmful for LevelDB either. To be on the safe side, don't risk changing
      // longstanding LevelDB behavior.
      // TODO(crbug.com/419203257): consider revisiting this logic along with
      // `CanOpportunisticallyClose()`.
      MaybeStartClosing();
    }
  }
}

void BucketContext::RunIdleTasks() {
  // Though the idle timer is stopped before resetting the backing store, an
  // already posted task may run after the backing store has been reset.
  if (!backing_store_) {
    return;
  }
  base::TimeTicks start = base::TimeTicks::Now();
  backing_store()->RunIdleTasks();
  base::TimeTicks end = base::TimeTicks::Now();
  LogDuration(end - start, "IndexedDB.BackendDuration.RunIdleTasks",
              in_memory());
  last_idle_tasks_completion_time_ = end;
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
  base::ElapsedTimer timer;
  if (!backing_store_) {
    Status s;
    DatabaseError error;
    std::tie(s, error, std::ignore) =
        InitBackingStore(/*create_if_missing=*/false);
    if (!s.ok()) {
      // Since `create_if_missing` is false, "not found" is a valid, non-error
      // status.
      CHECK_EQ(s.IsNotFound(),
               error.code() == blink::mojom::IDBException::kNoError)
          << error.code();
      std::move(callback).Run(
          {}, blink::mojom::IDBError::New(error.code(), error.message()));

      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
      }
      return;
    }
  }

  auto names_and_versions = LOG_RESULT(
      backing_store()->GetDatabaseNamesAndVersions(),
      "IndexedDB.BackingStore.GetDatabaseNamesAndVersions", in_memory());
  if (!names_and_versions.has_value()) {
    std::move(callback).Run({}, blink::mojom::IDBError::New(
                                    blink::mojom::IDBException::kUnknownError,
                                    u"Internal error opening backing store for "
                                    "indexedDB.databases()."));
    return;
  }
  LogDuration(timer.Elapsed(), "IndexedDB.BackendDuration.GetDatabaseInfo",
              in_memory());
  std::move(callback).Run(
      std::move(*names_and_versions),
      blink::mojom::IDBError::New(blink::mojom::IDBException::kNoError,
                                  std::u16string()));
}

void BucketContext::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const std::u16string& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    int scheduling_priority) {
  base::ElapsedTimer timer;
  TRACE_EVENT0("IndexedDB", "BucketContext::Open");

  if (version < 1 && version != blink::IndexedDBDatabaseMetadata::NO_VERSION) {
    mojo::ReportBadMessage("Invalid version");
    return;
  }

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> factory_client(
      std::move(pending_factory_client));

  IndexedDBDataLossInfo data_loss_info;
  if (!backing_store_) {
    Status s;
    DatabaseError error;
    std::tie(s, error, data_loss_info) =
        InitBackingStore(/*create_if_missing=*/true);
    LogStatus(s, "IndexedDB.BackingStore.CreateIfMissing", in_memory());
    if (!s.ok()) {
      std::move(factory_client)->Error(error.code(), error.message());
      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
      }
      return;
    }
  }

  auto connection = std::make_unique<PendingConnection>(
      std::move(factory_client),
      std::make_unique<DatabaseCallbacks>(std::move(database_callbacks_remote)),
      transaction_id, version, std::move(transaction_receiver));
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
    // The database must be added before the schedule call, as the
    // CreateDatabaseDeleteClosure can be called synchronously.
    database_ptr = CreateAndAddDatabase(name);
  } else {
    database_ptr = it->second.get();
  }

  database_ptr->ScheduleOpenConnection(std::move(connection), timer.Elapsed());
}

void BucketContext::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
        pending_factory_client,
    const std::u16string& name,
    bool force_close) {
  base::ElapsedTimer timer;
  TRACE_EVENT0("IndexedDB", "BucketContext::DeleteDatabase");
  mojo::AssociatedRemote<blink::mojom::IDBFactoryClient> factory_client(
      std::move(pending_factory_client));

  if (!backing_store_) {
    Status s;
    DatabaseError error;
    // Note: Any data loss information here is not piped up to the renderer, and
    // will be lost.
    std::tie(s, error, std::ignore) = InitBackingStore(
        /*create_if_missing=*/false);
    if (!s.ok()) {
      if (s.IsNotFound()) {
        // The spec requires oldVersion to be 0 if the database does not exist:
        // https://w3c.github.io/IndexedDB/#delete-a-database.
        std::move(factory_client)->DeleteSuccess(/*old_version=*/0);
        return;
      }

      std::move(factory_client)->Error(error.code(), error.message());
      if (s.IsCorruption()) {
        HandleBackingStoreCorruption(base::UTF16ToUTF8(error.message()));
      }
      return;
    }
  }

  Database* database = nullptr;
  if (auto it = databases_.find(name); it == databases_.end()) {
    // This adds `Database` in an uninitialized state.
    database = CreateAndAddDatabase(name);
  } else {
    database = it->second.get();
    if (force_close) {
      database->ForceCloseConnectionsAndCancelRequests(
          "The database is deleted.");
    }
  }
  database->ScheduleDeleteDatabase(std::move(factory_client),
                                   /*on_deletion_complete=*/
                                   base::BindOnce(delegate().on_files_written,
                                                  /*flushed=*/true),
                                   timer.Elapsed());
}

storage::mojom::IdbBucketMetadataPtr BucketContext::FillInMetadata(
    storage::mojom::IdbBucketMetadataPtr info) {
  // TODO(jsbell): Sort by name?
  std::vector<storage::mojom::IdbDatabaseMetadataPtr> database_list;
  info->size = GetUsage(/*write_in_progress=*/false);
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
  return this;
}

void BucketContext::FlushBackingStoreForTesting() {
  backing_store()->FlushForTesting();
}

void BucketContext::BindMockFailureSingletonForTesting(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> receiver) {
  level_db::BindMockFailureSingletonForTesting(std::move(receiver));  // IN-TEST
}

Database* BucketContext::CreateAndAddDatabase(const std::u16string& name) {
  CHECK(!databases_.contains(name));
  auto database = std::make_unique<Database>(name, *this);
  return databases_.emplace(name, std::move(database)).first->second.get();
}

void BucketContext::OnHandleCreated() {
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
  CHECK_GT(open_handles_, 0ll);
  --open_handles_;
  MaybeStartClosing();
}

bool BucketContext::CanClose() {
  CHECK_GE(open_handles_, 0);

  if (backing_store_ && !skip_closing_sequence_ &&
      !backing_store()->CanOpportunisticallyClose()) {
    return false;
  }

  return !has_blobs_outstanding_ && open_handles_ <= 0 &&
         (!backing_store_ || is_doomed_ || !in_memory());
}

void BucketContext::MaybeStartClosing() {
  if (!IsClosing() && CanClose()) {
    StartClosing();
  }
}

void BucketContext::StartClosing() {
  CHECK(CanClose());
  CHECK(!IsClosing());

  if (skip_closing_sequence_) {
    CloseNow();
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  CHECK(!close_timer_.IsRunning());
  closing_stage_ = ClosingState::kPreCloseGracePeriod;
  close_timer_.Start(FROM_HERE, base::Seconds(kBackingStoreGracePeriodSeconds),
                     base::BindOnce(&BucketContext::StartPreCloseTasks,
                                    weak_factory_.GetWeakPtr()));
}

void BucketContext::StartPreCloseTasks() {
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
  file_reader_map_.erase(path);
}

std::string BucketContext::SanitizeErrorMessage(const std::string& message) {
  // The message may contain the database path, which may be considered
  // sensitive data, and those strings are passed to the extension, so strip it.
  std::string sanitized_message = message;
  base::ReplaceSubstringsAfterOffset(&sanitized_message, 0u,
                                     data_path_.AsUTF8Unsafe(), "...");
  return sanitized_message;
}

// static
base::AutoReset<std::optional<bool>>
BucketContext::OverrideShouldUseSqliteForTesting(bool use_sqlite) {
  CHECK(!g_should_use_sqlite_for_testing.has_value());
  base::AutoReset<std::optional<bool>> scoped_override(
      &g_should_use_sqlite_for_testing, use_sqlite);
  return scoped_override;
}

void BucketContext::SetSqliteRolloutStageForTesting(SqliteRolloutStage stage) {
  CHECK(!backing_store_);
  sqlite_rollout_stage_ = stage;
}

// static
void BucketContext::InsertTeardownStepForTesting(
    base::OnceClosure on_teardown) {
  GetTeardownExtraStepForTesting() = std::move(on_teardown);
}

// static
base::TimeDelta BucketContext::GetIdleTimeoutForTesting() {
  return kIdleTimeout;
}

void BucketContext::HandleBackingStoreCorruption(
    const std::string& error_message) {
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

void BucketContext::OnDatabaseError(Database* database,
                                    Status status,
                                    const std::string& message) {
  CHECK(!status.ok());

  if (status.IsIOError()) {
    quota_manager_proxy_->OnClientWriteFailed(bucket_info_.storage_key);
  }

  const std::string error_message =
      message.empty() ? status.ToString() : message;
  if (IsUsingSqlite()) {
    // Unlike in the LevelDB case, an error in one database doesn't indicate a
    // problem with the entire bucket, so we just `ForceClose` the one
    // `Database`.
    CHECK(database);
    auto iter = databases_.find(database->name());
    CHECK(iter != databases_.end());
    iter->second->ForceCloseConnectionsAndCancelRequests(error_message);
    CHECK(iter->second->CanBeDestroyed());
    databases_.erase(iter);
  } else {
    if (status.IsCorruption()) {
      HandleBackingStoreCorruption(error_message);
      return;
    }
    ForceClose(/*doom=*/false, error_message);
  }
}

bool BucketContext::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                 base::trace_event::ProcessMemoryDump* pmd) {
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
BucketContext::InitBackingStore(bool create_if_missing) {
  CHECK(!backing_store_);
  bool should_use_sqlite =
      ShouldUseSqlite(sqlite_rollout_stage_, bucket_locator(), data_path_);

  // Construct paths and create required directories.
  base::FilePath blob_path;
  base::FilePath database_path;
  if (!in_memory()) {
    // Creates the base directory if necessary, e.g.
    // <user-data-dir>/<profile-name>/IndexedDB/
    if (!base::CreateDirectory(data_path_)) {
      ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                       bucket_locator());
      return {Status::IOError("Unable to create IndexedDB data path"),
              CreateDefaultError(), IndexedDBDataLossInfo()};
    }

    base::FilePath sqlite_database_path =
        data_path_.Append(GetSqliteDbDirectory(bucket_locator()));
    base::FilePath leveldb_database_path =
        data_path_.Append(GetLevelDBFileName(bucket_locator()));
    base::FilePath leveldb_blob_path =
        data_path_.Append(GetBlobStoreFileName(bucket_locator()));

    if (should_use_sqlite) {
      // Construct the directory path where databases are stored, e.g.
      // <user-data-dir>/<profile-name>/IndexedDB/https_example.com/
      database_path = sqlite_database_path;
      base::DeletePathRecursively(leveldb_database_path);
      base::DeletePathRecursively(leveldb_blob_path);
    } else {
      database_path = leveldb_database_path;
      blob_path = leveldb_blob_path;
      if (sqlite_database_path.IsParent(leveldb_database_path)) {
        // True for non-legacy buckets. Delete everything except the leveldb and
        // blob directories.
        base::FileEnumerator enumerator(sqlite_database_path,
                                        /*recursive=*/false,
                                        base::FileEnumerator::NAMES_ONLY);
        enumerator.ForEach([&](const base::FilePath& path) {
          if (path != leveldb_database_path && path != leveldb_blob_path) {
            base::DeletePathRecursively(path);
          }
        });
      } else {
        base::DeletePathRecursively(sqlite_database_path);
      }

#if BUILDFLAG(IS_WIN)
      int max_ldb_file_path_length =
          // The longest file path LevelDB uses.
          leveldb_database_path.AppendASCII("MANIFEST-000001").value().size();

      // Underflow (i.e. file path length <= MAX_PATH) intentionally emits to
      // the 0 bucket.
      base::UmaHistogramCounts100("IndexedDB.FilePathLengthOverflow.LevelDB",
                                  max_ldb_file_path_length - MAX_PATH);

      int max_sqlite_file_path_length =
          sqlite_database_path
              // All database names hash to the same length file name.
              .Append(DatabaseNameToFileName(u"any_string"))
              // The WAL file will use the path with "-wal" appended. This
              // appends ".wal".
              .AddExtensionASCII("wal")
              .value()
              .size();

      // Underflow (i.e. file path length <= MAX_PATH) intentionally emits to
      // the 0 bucket.
      base::UmaHistogramCounts100("IndexedDB.FilePathLengthOverflow.SQLite",
                                  max_sqlite_file_path_length - MAX_PATH);
#endif
    }

    if (IsPathTooLong(database_path)) {
      ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
                       bucket_locator());
      return {Status::IOError("File path too long"), CreateDefaultError(),
              IndexedDBDataLossInfo()};
    }
    if (should_use_sqlite && base::IsDirectoryEmpty(database_path)) {
      if (!create_if_missing) {
        return {Status::NotFound("Backing store does not exist"),
                DatabaseError(), IndexedDBDataLossInfo()};
      }
      if (!base::CreateDirectory(database_path)) {
        ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
                         bucket_locator());
        return {Status::IOError("Unable to create IndexedDB database path"),
                CreateDefaultError(), IndexedDBDataLossInfo()};
      }
    }
  }

  auto lock_manager = std::make_unique<PartitionedLockManager>();
  IndexedDBDataLossInfo data_loss_info;

  if (should_use_sqlite) {
    backing_store_.emplace(
        std::make_unique<sqlite::BackingStoreImpl>(
            database_path, *blob_storage_context_,
            base::BindRepeating(
                [](PartitionedLockManager& lock_manager,
                   const std::u16string& name) {
                  // TODO(crbug.com/436880909): Deduplicate with
                  // `BuildLockRequestsForSqlite()`.
                  std::string key = DatabaseNameToFileName(name).MaybeAsASCII();
                  constexpr int kMetadataLockPartition = 0;
                  PartitionedLockHolder lock_holder;
                  lock_manager.AcquireLocks(
                      {{{kMetadataLockPartition, key},
                        PartitionedLockManager::LockType::kExclusive}},
                      lock_holder, base::DoNothing());
                  // Locks should be granted synchronously.
                  CHECK_EQ(lock_holder.locks.size(), 1U);
                  return std::move(lock_holder.locks);
                },
                std::ref(*lock_manager))),
        /*is_sqlite=*/true);
  } else {
    bool create_sqlite_if_missing =
        !in_memory() && create_if_missing &&
        sqlite_rollout_stage_ == SqliteRolloutStage::kUseSqliteForNewStores;
    std::unique_ptr<BackingStore> backing_store;
    bool disk_full = false;
    Status status, first_try_status;
    constexpr static const int kNumOpenTries = 2;
    for (int i = 0; i < kNumOpenTries; ++i) {
      const bool is_first_attempt = i == 0;
      std::tie(backing_store, status, data_loss_info, disk_full) =
          level_db::BackingStore::OpenAndVerify(
              *this, data_path_, database_path, blob_path, lock_manager.get(),
              is_first_attempt,
              /*create_if_missing=*/create_if_missing &&
                  !create_sqlite_if_missing);
      CHECK_EQ(status.ok(), !!backing_store);
      if (is_first_attempt) [[likely]] {
        first_try_status = status;
      }
      if (status.ok()) [[likely]] {
        break;
      }
      CHECK(!backing_store);
      if (status.IsNotFound()) {
        if (create_sqlite_if_missing) {
          // Clear out stale files that may have been left behind.
          base::DeletePathRecursively(database_path);
          base::DeletePathRecursively(blob_path);
          // Preserve and pass on data loss info.
          auto result = InitBackingStore(/*create_if_missing=*/true);
          std::get<IndexedDBDataLossInfo>(result) = data_loss_info;
          return result;
        }
        if (!create_if_missing) {
          return {status, DatabaseError(), data_loss_info};
        }
      }
      // If the disk is full, always exit immediately.
      if (disk_full) {
        break;
      }
    }

    // Record this here because the !create_if_missing && not_found case
    // shouldn't count as either a success or failure.
    base::UmaHistogramEnumeration(kBackingStoreActionUmaName,
                                  IndexedDBAction::kBackingStoreOpenAttempt);

    first_try_status.LogLevelDbStatus(
        "WebCore.IndexedDB.BackingStore.OpenFirstTryResult");

    if (!status.ok()) [[unlikely]] {
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

    backing_store_.emplace(std::move(backing_store), /*is_sqlite=*/false);
  }

  if (!in_memory()) {
    ReportOpenStatus(INDEXED_DB_BACKING_STORE_OPEN_SUCCESS, bucket_locator());
  }

  lock_manager_ = std::move(lock_manager);
  delegate().on_files_written.Run(/*flushed=*/true);
  return {Status::OK(), DatabaseError(), data_loss_info};
}

void BucketContext::ResetBackingStore() {
  file_reader_map_.clear();
  weak_factory_.InvalidateWeakPtrs();
  idle_timer_.Stop();

  std::optional<bool> was_using_sqlite;
  if (backing_store_) {
    was_using_sqlite = IsUsingSqlite();
    base::WaitableEvent destruct_event;
    std::move(*backing_store()).SignalWhenDestructionComplete(&destruct_event);
    backing_store_.reset();
    destruct_event.Wait();
    if (!GetTeardownExtraStepForTesting().is_null()) {
      std::move(GetTeardownExtraStepForTesting()).Run();
    }
  }

  if (is_doomed_ && !in_memory()) {
    // TODO(crbug.com/436887363): Log if deletion fails.
    if (ShouldUseLegacyFilePath(bucket_locator())) {
      if (was_using_sqlite.value_or(ShouldUseSqlite(
              sqlite_rollout_stage_, bucket_locator(), data_path_))) {
        base::DeletePathRecursively(
            data_path_.Append(GetSqliteDbDirectory(bucket_locator())));
      } else {
        base::DeletePathRecursively(
            data_path_.Append(GetLevelDBFileName(bucket_locator())));
        base::DeletePathRecursively(
            data_path_.Append(GetBlobStoreFileName(bucket_locator())));
      }
    } else {
      base::DeletePathRecursively(data_path_);
    }
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
