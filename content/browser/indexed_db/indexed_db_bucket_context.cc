// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_bucket_context.h"

#include <list>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/base_tracing.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_compaction_task.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"
#include "content/browser/indexed_db/indexed_db_tombstone_sweeper.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
namespace {
// Time after the last connection to a database is closed and when we destroy
// the backing store.
const int64_t kBackingStoreGracePeriodSeconds = 2;
// Total time we let pre-close tasks run.
const int64_t kRunningPreCloseTasksMaxRunPeriodSeconds = 60;
// The number of iterations for every 'round' of the tombstone sweeper.
const int kTombstoneSweeperRoundIterations = 1000;
// The maximum total iterations for the tombstone sweeper.
const int kTombstoneSweeperMaxIterations = 10 * 1000 * 1000;

constexpr const base::TimeDelta kMinEarliestBucketSweepFromNow = base::Days(1);
static_assert(kMinEarliestBucketSweepFromNow <
                  IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow,
              "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalSweepFromNow =
    base::Minutes(5);
static_assert(kMinEarliestGlobalSweepFromNow <
                  IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow,
              "Min < Max");

base::Time GenerateNextBucketSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow.InMilliseconds() -
      kMinEarliestBucketSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestBucketSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

base::Time GenerateNextGlobalSweepTime(base::Time now) {
  uint64_t range =
      IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow.InMilliseconds() -
      kMinEarliestGlobalSweepFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalSweepFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

constexpr const base::TimeDelta kMinEarliestBucketCompactionFromNow =
    base::Days(1);
static_assert(kMinEarliestBucketCompactionFromNow <
                  IndexedDBBucketContext::kMaxEarliestBucketCompactionFromNow,
              "Min < Max");

constexpr const base::TimeDelta kMinEarliestGlobalCompactionFromNow =
    base::Minutes(5);
static_assert(kMinEarliestGlobalCompactionFromNow <
                  IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow,
              "Min < Max");

base::Time GenerateNextBucketCompactionTime(base::Time now) {
  uint64_t range = IndexedDBBucketContext::kMaxEarliestBucketCompactionFromNow
                       .InMilliseconds() -
                   kMinEarliestBucketCompactionFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestBucketCompactionFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

base::Time GenerateNextGlobalCompactionTime(base::Time now) {
  uint64_t range = IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow
                       .InMilliseconds() -
                   kMinEarliestGlobalCompactionFromNow.InMilliseconds();
  int64_t rand_millis = kMinEarliestGlobalCompactionFromNow.InMilliseconds() +
                        static_cast<int64_t>(base::RandGenerator(range));
  return now + base::Milliseconds(rand_millis);
}

}  // namespace

// BlobDataItemReader implementation providing a BlobDataItem -> file adapter.
class IndexedDBDataItemReader : public storage::mojom::BlobDataItemReader {
 public:
  IndexedDBDataItemReader(
      const base::FilePath& file_path,
      base::Time expected_modification_time,
      base::OnceClosure release_callback,
      base::OnceCallback<void(const base::FilePath&)>
          on_last_receiver_disconnected,
      scoped_refptr<base::TaskRunner> file_task_runner,
      scoped_refptr<base::TaskRunner> io_task_runner,
      mojo::PendingReceiver<storage::mojom::BlobDataItemReader>
          initial_receiver)
      : file_path_(file_path),
        expected_modification_time_(std::move(expected_modification_time)),
        release_callback_(std::move(release_callback)),
        on_last_receiver_disconnected_(
            std::move(on_last_receiver_disconnected)),
        file_task_runner_(std::move(file_task_runner)),
        io_task_runner_(std::move(io_task_runner)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(file_task_runner_);
    DCHECK(io_task_runner_);

    AddReader(std::move(initial_receiver));

    // The `BlobStorageContext` will disconnect when the blob is no longer
    // referenced.
    receivers_.set_disconnect_handler(
        base::BindRepeating(&IndexedDBDataItemReader::OnClientDisconnected,
                            base::Unretained(this)));
  }

  IndexedDBDataItemReader(const IndexedDBDataItemReader&) = delete;
  IndexedDBDataItemReader& operator=(const IndexedDBDataItemReader&) = delete;

  ~IndexedDBDataItemReader() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(release_callback_).Run();
  }

  void AddReader(mojo::PendingReceiver<BlobDataItemReader> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(receiver.is_valid());

    receivers_.Add(this, std::move(receiver));
  }

  void Read(uint64_t offset,
            uint64_t length,
            mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto reader = storage::FileStreamReader::CreateForLocalFile(
        file_task_runner_.get(), file_path_, offset,
        expected_modification_time_);
    auto adapter = std::make_unique<FileStreamReaderToDataPipe>(
        std::move(reader), std::move(pipe));
    auto* raw_adapter = adapter.get();

    // Have the adapter (owning the reader) be owned by the result callback.
    auto current_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    auto result_callback = base::BindOnce(
        [](std::unique_ptr<FileStreamReaderToDataPipe> reader,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           ReadCallback callback, int result) {
          // |callback| is expected to be run on the original sequence
          // that called this Read function, so post it back.
          task_runner->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), result));
        },
        std::move(adapter), std::move(current_task_runner),
        std::move(callback));

    // On Windows, all async file IO needs to be done on the io thread.
    // Do this on all platforms for consistency, even if not necessary on posix.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FileStreamReaderToDataPipe* adapter, uint64_t length,
               base::OnceCallback<void(int)> result_callback) {
              adapter->Start(std::move(result_callback), length);
            },
            // |raw_adapter| is owned by |result_callback|.
            base::Unretained(raw_adapter), length, std::move(result_callback)));
  }

  void ReadSideData(ReadSideDataCallback callback) override {
    // This type should never have side data.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(net::ERR_NOT_IMPLEMENTED, mojo_base::BigBuffer());
  }

 private:
  void OnClientDisconnected() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!receivers_.empty()) {
      return;
    }

    std::move(on_last_receiver_disconnected_).Run(file_path_);
    // `this` is deleted.
  }

  mojo::ReceiverSet<storage::mojom::BlobDataItemReader> receivers_;

  base::FilePath file_path_;
  base::Time expected_modification_time_;

  // Called on destruction. TODO(estade): can this be combined with
  // `on_last_receiver_disconnected_`?
  base::OnceClosure release_callback_;

  // Called when the last receiver is disconnected. Will destroy `this`.
  base::OnceCallback<void(const base::FilePath&)>
      on_last_receiver_disconnected_;

  // There are a lot of task runners in this class:
  // * IndexedDBDataItemReader runs on the same sequence as
  // `IndexedDBBucketContext`.
  // * LocalFileStreamReader wants its own |file_task_runner_| to run
  //   various asynchronous file operations on.
  // * net::FileStream (used by LocalFileStreamReader) needs to be run
  //   on an IO thread for asynchronous file operations (on Windows), which
  //   is done by passing in an |io_task_runner| to do this.
  // TODO(estade): can these be simplified?
  scoped_refptr<base::TaskRunner> file_task_runner_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

constexpr const base::TimeDelta
    IndexedDBBucketContext::kMaxEarliestGlobalSweepFromNow;
constexpr const base::TimeDelta
    IndexedDBBucketContext::kMaxEarliestBucketSweepFromNow;

constexpr const base::TimeDelta
    IndexedDBBucketContext::kMaxEarliestGlobalCompactionFromNow;
constexpr const base::TimeDelta
    IndexedDBBucketContext::kMaxEarliestBucketCompactionFromNow;

IndexedDBBucketContext::Delegate::Delegate()
    : on_fatal_error(base::DoNothing()),
      on_ready_for_destruction(base::DoNothing()),
      on_content_changed(base::DoNothing()),
      on_writing_transaction_complete(base::DoNothing()),
      for_each_bucket_context(base::DoNothing()) {}

IndexedDBBucketContext::Delegate::Delegate(Delegate&& other) = default;
IndexedDBBucketContext::Delegate::~Delegate() = default;

IndexedDBBucketContext::IndexedDBBucketContext(
    storage::BucketInfo bucket_info,
    std::unique_ptr<PartitionedLockManager> lock_manager,
    Delegate&& delegate,
    std::unique_ptr<IndexedDBBackingStore> backing_store,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    scoped_refptr<base::TaskRunner> io_task_runner,
    mojo::PendingRemote<storage::mojom::BlobStorageContext>
        blob_storage_context,
    mojo::PendingRemote<storage::mojom::FileSystemAccessContext>
        file_system_access_context,
    InstanceClosure initialize_closure)
    : bucket_info_(std::move(bucket_info)),
      lock_manager_(std::move(lock_manager)),
      backing_store_(std::move(backing_store)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      file_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      io_task_runner_(std::move(io_task_runner)),
      blob_storage_context_(std::move(blob_storage_context)),
      file_system_access_context_(std::move(file_system_access_context)),
      delegate_(std::move(delegate)) {

  backing_store_->set_bucket_context(this);

  if (!initialize_closure) {
    base::Time now = base::Time::Now();
    initialize_closure =
        base::BindRepeating(&IndexedDBBucketContext::SetInternalState,
                            GenerateNextGlobalSweepTime(now),
                            GenerateNextGlobalCompactionTime(now));
    delegate_.for_each_bucket_context.Run(initialize_closure);
  }
  initialize_closure.Run(*this);
}

IndexedDBBucketContext::~IndexedDBBucketContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backing_store_) {
    return;
  }
  if (backing_store_->IsBlobCleanupPending()) {
    backing_store_->ForceRunBlobCleanup();
  }

  base::WaitableEvent leveldb_destruct_event;
  backing_store_->TearDown(&leveldb_destruct_event);
  backing_store_.reset();
  leveldb_destruct_event.Wait();
}

void IndexedDBBucketContext::ForceClose(bool doom) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_doomed_ = doom;

  {
    // This handle keeps `this` from closing until it goes out of scope.
    IndexedDBBucketContextHandle handle(*this);
    for (const auto& pair : databases_) {
      // Note: We purposefully ignore the result here as force close needs to
      // continue tearing things down anyways.
      pair.second->ForceCloseAndRunTasks();
    }
    databases_.clear();
    if (has_blobs_outstanding_) {
      backing_store_->active_blob_registry()->ForceShutdown();
      has_blobs_outstanding_ = false;
    }

    // Don't run the preclosing tasks after a ForceClose, whether or not we've
    // started them.  Compaction in particular can run long and cannot be
    // interrupted, so it can cause shutdown hangs.
    close_timer_.AbandonAndStop();
    if (pre_close_task_queue_) {
      pre_close_task_queue_->Stop(
          IndexedDBPreCloseTaskQueue::StopReason::FORCE_CLOSE);
      pre_close_task_queue_.reset();
    }
    skip_closing_sequence_ = true;
  }

  // Initiate deletion if appropriate.
  RunTasks();
}

void IndexedDBBucketContext::ReportOutstandingBlobs(bool blobs_outstanding) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blobs_outstanding_ = blobs_outstanding;
  MaybeStartClosing();
}

void IndexedDBBucketContext::RunInstanceClosure(InstanceClosure method) {
  method.Run(*this);
}

void IndexedDBBucketContext::CheckCanUseDiskSpace(
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
    quota_manager()->GetBucketSpaceRemaining(
        bucket_locator(), backing_store_->idb_task_runner(),
        base::BindOnce(&IndexedDBBucketContext::OnGotBucketSpaceRemaining,
                       weak_factory_.GetWeakPtr()));
  }
}

void IndexedDBBucketContext::OnGotBucketSpaceRemaining(
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

int64_t IndexedDBBucketContext::GetBucketSpaceToAllot() {
  base::TimeDelta bucket_space_age =
      base::TimeTicks::Now() - bucket_space_remaining_timestamp_;
  if (bucket_space_age > kBucketSpaceCacheTimeLimit) {
    return 0;
  }
  return bucket_space_remaining_ *
         (1 - bucket_space_age / kBucketSpaceCacheTimeLimit);
}

void IndexedDBBucketContext::CreateAllExternalObjects(
    const std::vector<IndexedDBExternalObject>& objects,
    std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("IndexedDB", "IndexedDBBucketContext::CreateAllExternalObjects");

  DCHECK_EQ(objects.size(), mojo_objects->size());
  if (objects.empty()) {
    return;
  }

  for (size_t i = 0; i < objects.size(); ++i) {
    auto& blob_info = objects[i];
    auto& mojo_object = (*mojo_objects)[i];

    switch (blob_info.object_type()) {
      case IndexedDBExternalObject::ObjectType::kBlob:
      case IndexedDBExternalObject::ObjectType::kFile: {
        DCHECK(mojo_object->is_blob_or_file());
        auto& output_info = mojo_object->get_blob_or_file();

        auto receiver = output_info->blob.InitWithNewPipeAndPassReceiver();
        if (blob_info.is_remote_valid()) {
          output_info->uuid = blob_info.uuid();
          blob_info.Clone(std::move(receiver));
          continue;
        }

        auto element = storage::mojom::BlobDataItem::New();
        // TODO(enne): do we have to handle unknown size here??
        element->size = blob_info.size();
        element->side_data_size = 0;
        element->content_type = base::UTF16ToUTF8(blob_info.type());
        element->type = storage::mojom::BlobDataItemType::kIndexedDB;

        base::Time last_modified;
        // Android doesn't seem to consistently be able to set file modification
        // times. https://crbug.com/1045488
#if !BUILDFLAG(IS_ANDROID)
        last_modified = blob_info.last_modified();
#endif
        BindFileReader(blob_info.indexed_db_file_path(), last_modified,
                       blob_info.release_callback(),
                       element->reader.InitWithNewPipeAndPassReceiver());

        // Write results to output_info.
        output_info->uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

        blob_storage_context_->RegisterFromDataItem(
            std::move(receiver), output_info->uuid, std::move(element));
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

void IndexedDBBucketContext::QueueRunTasks() {
  if (task_run_queued_) {
    return;
  }

  task_run_queued_ = true;
  backing_store_->idb_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IndexedDBBucketContext::RunTasks,
                                weak_factory_.GetWeakPtr()));
}

void IndexedDBBucketContext::RunTasks() {
  task_run_queued_ = false;

  leveldb::Status status;
  for (auto db_it = databases_.begin(); db_it != databases_.end();) {
    IndexedDBDatabase& db = *db_it->second;

    IndexedDBDatabase::RunTasksResult tasks_result;
    std::tie(tasks_result, status) = db.RunTasks();
    switch (tasks_result) {
      case IndexedDBDatabase::RunTasksResult::kDone:
        ++db_it;
        continue;

      case IndexedDBDatabase::RunTasksResult::kError:
        delegate().on_fatal_error.Run(status);
        return;

      case IndexedDBDatabase::RunTasksResult::kCanBeDestroyed:
        databases_.erase(db_it);
        break;
    }
  }
  if (CanClose() && closing_stage_ == ClosingState::kClosed) {
    return delegate().on_ready_for_destruction.Run();
  }
}

// static
void IndexedDBBucketContext::SetInternalState(
    base::Time earliest_global_sweep_time,
    base::Time earliest_global_compaction_time,
    IndexedDBBucketContext& context) {
  if (!earliest_global_sweep_time.is_null()) {
    context.earliest_global_sweep_time_ = earliest_global_sweep_time;
  }
  if (!earliest_global_compaction_time.is_null()) {
    context.earliest_global_compaction_time_ = earliest_global_compaction_time;
  }
}

IndexedDBDatabase* IndexedDBBucketContext::AddDatabase(
    const std::u16string& name,
    std::unique_ptr<IndexedDBDatabase> database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(databases_, name));
  return databases_.emplace(name, std::move(database)).first->second.get();
}

void IndexedDBBucketContext::OnHandleCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++open_handles_;
  if (closing_stage_ != ClosingState::kNotClosing) {
    closing_stage_ = ClosingState::kNotClosing;
    close_timer_.AbandonAndStop();
    if (pre_close_task_queue_) {
      pre_close_task_queue_->Stop(
          IndexedDBPreCloseTaskQueue::StopReason::NEW_CONNECTION);
      pre_close_task_queue_.reset();
    }
  }
}

void IndexedDBBucketContext::OnHandleDestruction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(open_handles_, 0ll);
  --open_handles_;
  MaybeStartClosing();
}

bool IndexedDBBucketContext::CanClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(open_handles_, 0);
  return !has_blobs_outstanding_ && open_handles_ <= 0 &&
         (is_doomed_ || !backing_store_->in_memory());
}

void IndexedDBBucketContext::MaybeStartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsClosing() && CanClose()) {
    StartClosing();
  }
}

void IndexedDBBucketContext::StartClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanClose());
  DCHECK(!IsClosing());

  if (skip_closing_sequence_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIDBCloseImmediatelySwitch)) {
    CloseNow();
    return;
  }

  // Start a timer to close the backing store, unless something else opens it
  // in the mean time.
  DCHECK(!close_timer_.IsRunning());
  closing_stage_ = ClosingState::kPreCloseGracePeriod;
  close_timer_.Start(
      FROM_HERE, base::Seconds(kBackingStoreGracePeriodSeconds),
      base::BindOnce(
          [](base::WeakPtr<IndexedDBBucketContext> bucket_context) {
            if (!bucket_context || bucket_context->closing_stage_ !=
                                       ClosingState::kPreCloseGracePeriod) {
              return;
            }
            bucket_context->StartPreCloseTasks();
          },
          weak_factory_.GetWeakPtr()));
}

void IndexedDBBucketContext::StartPreCloseTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(closing_stage_ == ClosingState::kPreCloseGracePeriod);
  closing_stage_ = ClosingState::kRunningPreCloseTasks;

  // The callback will run on all early returns in this function.
  base::ScopedClosureRunner maybe_close_backing_store_runner(base::BindOnce(
      [](base::WeakPtr<IndexedDBBucketContext> bucket_context) {
        if (!bucket_context || bucket_context->closing_stage_ !=
                                   ClosingState::kRunningPreCloseTasks) {
          return;
        }
        bucket_context->CloseNow();
      },
      weak_factory_.GetWeakPtr()));

  std::list<std::unique_ptr<IndexedDBPreCloseTaskQueue::PreCloseTask>> tasks;

  if (ShouldRunTombstoneSweeper()) {
    tasks.push_back(std::make_unique<IndexedDBTombstoneSweeper>(
        kTombstoneSweeperRoundIterations, kTombstoneSweeperMaxIterations,
        backing_store_->db()->db()));
  }

  if (ShouldRunCompaction()) {
    tasks.push_back(
        std::make_unique<IndexedDBCompactionTask>(backing_store_->db()->db()));
  }

  if (!tasks.empty()) {
    pre_close_task_queue_ = std::make_unique<IndexedDBPreCloseTaskQueue>(
        std::move(tasks), maybe_close_backing_store_runner.Release(),
        base::Seconds(kRunningPreCloseTasksMaxRunPeriodSeconds),
        std::make_unique<base::OneShotTimer>());
    pre_close_task_queue_->Start(
        base::BindOnce(&IndexedDBBackingStore::GetCompleteMetadata,
                       base::Unretained(backing_store_.get())));
  }
}

void IndexedDBBucketContext::CloseNow() {
  closing_stage_ = ClosingState::kClosed;
  close_timer_.AbandonAndStop();
  pre_close_task_queue_.reset();
  QueueRunTasks();
}

bool IndexedDBBucketContext::ShouldRunTombstoneSweeper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time now = base::Time::Now();
  // Check that the last sweep hasn't run too recently.
  if (earliest_global_sweep_time_ > now) {
    return false;
  }

  base::Time bucket_earliest_sweep;
  leveldb::Status s = indexed_db::GetEarliestSweepTime(backing_store_->db(),
                                                       &bucket_earliest_sweep);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok() && !s.IsNotFound()) {
    return false;
  }

  if (bucket_earliest_sweep > now) {
    return false;
  }

  // A sweep will happen now, so reset the sweep timers.
  earliest_global_sweep_time_ = GenerateNextGlobalSweepTime(now);
  delegate().for_each_bucket_context.Run(base::BindRepeating(
      &IndexedDBBucketContext::SetInternalState, earliest_global_sweep_time_,
      earliest_global_compaction_time_));
  std::unique_ptr<LevelDBDirectTransaction> txn =
      IndexedDBClassFactory::Get()
          ->transactional_leveldb_factory()
          .CreateLevelDBDirectTransaction(backing_store_->db());
  s = indexed_db::SetEarliestSweepTime(txn.get(),
                                       GenerateNextBucketSweepTime(now));
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok()) {
    return false;
  }
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok()) {
    return false;
  }
  return true;
}

bool IndexedDBBucketContext::ShouldRunCompaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time now = base::Time::Now();
  // Check that the last compaction hasn't run too recently.
  if (earliest_global_compaction_time_ > now) {
    return false;
  }

  base::Time bucket_earliest_compaction;
  leveldb::Status s = indexed_db::GetEarliestCompactionTime(
      backing_store_->db(), &bucket_earliest_compaction);
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok() && !s.IsNotFound()) {
    return false;
  }

  if (bucket_earliest_compaction > now) {
    return false;
  }

  // A compaction will happen now, so reset the compaction timers.
  earliest_global_compaction_time_ = GenerateNextGlobalCompactionTime(now);
  delegate().for_each_bucket_context.Run(base::BindRepeating(
      &IndexedDBBucketContext::SetInternalState, earliest_global_sweep_time_,
      earliest_global_compaction_time_));
  std::unique_ptr<LevelDBDirectTransaction> txn =
      IndexedDBClassFactory::Get()
          ->transactional_leveldb_factory()
          .CreateLevelDBDirectTransaction(backing_store_->db());
  s = indexed_db::SetEarliestCompactionTime(
      txn.get(), GenerateNextBucketCompactionTime(now));
  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok()) {
    return false;
  }
  s = txn->Commit();

  // TODO(dmurph): Log this or report to UMA.
  if (!s.ok()) {
    return false;
  }
  return true;
}

void IndexedDBBucketContext::BindFileReader(
    const base::FilePath& path,
    base::Time expected_modification_time,
    base::OnceClosure release_callback,
    mojo::PendingReceiver<storage::mojom::BlobDataItemReader> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(receiver.is_valid());
  DCHECK(file_task_runner_);

  auto itr = file_reader_map_.find(path);
  if (itr != file_reader_map_.end()) {
    itr->second->AddReader(std::move(receiver));
    return;
  }

  auto reader = std::make_unique<IndexedDBDataItemReader>(
      path, expected_modification_time, std::move(release_callback),
      base::BindOnce(&IndexedDBBucketContext::RemoveBoundReaders,
                     weak_factory_.GetWeakPtr()),
      file_task_runner_, io_task_runner_, std::move(receiver));
  file_reader_map_.insert({path, std::move(reader)});
}

void IndexedDBBucketContext::RemoveBoundReaders(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_reader_map_.erase(path);
}

}  // namespace content
