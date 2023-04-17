// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/transaction_impl.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// BlobDataItemReader implementation providing a BlobDataItem -> file adapter.
class IndexedDBDataItemReader : public storage::mojom::BlobDataItemReader {
 public:
  IndexedDBDataItemReader(
      IndexedDBDispatcherHost* host,
      const base::FilePath& file_path,
      base::Time expected_modification_time,
      base::RepeatingClosure release_callback,
      scoped_refptr<base::TaskRunner> file_task_runner,
      scoped_refptr<base::TaskRunner> io_task_runner,
      mojo::PendingReceiver<storage::mojom::BlobDataItemReader>
          initial_receiver)
      : host_(host),
        file_path_(file_path),
        expected_modification_time_(std::move(expected_modification_time)),
        release_callback_(std::move(release_callback)),
        file_task_runner_(std::move(file_task_runner)),
        io_task_runner_(std::move(io_task_runner)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(host);
    DCHECK(file_task_runner_);

    AddReader(std::move(initial_receiver));

    // Unretained(this) is safe because |this| owns |receivers_|.
    receivers_.set_disconnect_handler(
        base::BindRepeating(&IndexedDBDataItemReader::OnClientDisconnected,
                            base::Unretained(this)));
  }

  IndexedDBDataItemReader(const IndexedDBDataItemReader&) = delete;
  IndexedDBDataItemReader& operator=(const IndexedDBDataItemReader&) = delete;

  ~IndexedDBDataItemReader() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    release_callback_.Run();
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

    auto reader = storage::FileStreamReader::CreateForIndexedDBDataItemReader(
        file_task_runner_.get(), file_path_, storage::CreateFilesystemProxy(),
        offset, expected_modification_time_);
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
    if (!receivers_.empty())
      return;

    host_->RemoveBoundReaders(file_path_);
    // |this| is likely deleted at this point.
  }

  mojo::ReceiverSet<storage::mojom::BlobDataItemReader> receivers_;

  // |this| is owned by |host|, so this raw "client pointer" is safe.
  raw_ptr<IndexedDBDispatcherHost> host_;

  base::FilePath file_path_;
  base::Time expected_modification_time_;
  base::RepeatingClosure release_callback_;

  // There are a lot of task runners in this class:
  // * IndexedDBDataItemReader itself needs to run on the IDB sequence.
  //   This is because releasing a ref needs to be done synchronously when
  //   the mojo interface connection is broken to avoid racing with adding
  //   refs, and the active blob registry is on the IDB sequence.
  // * LocalFileStreamReader wants its own |file_task_runner_| to run
  //   various asynchronous file operations on.
  // * net::FileStream (used by LocalFileStreamReader) needs to be run
  //   on an IO thread for asynchronous file operations (on Windows), which
  //   is done by passing in an |io_task_runner| to do this.
  scoped_refptr<base::TaskRunner> file_task_runner_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IndexedDBContextImpl* indexed_db_context,
    scoped_refptr<base::TaskRunner> io_task_runner)
    : indexed_db_context_(indexed_db_context),
      io_task_runner_(std::move(io_task_runner)),
      file_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(indexed_db_context_);
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

IndexedDBDispatcherHost::ReceiverContext::ReceiverContext() = default;
IndexedDBDispatcherHost::ReceiverContext::ReceiverContext(
    absl::optional<storage::BucketInfo> bucket,
    mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
        client_state_checker_remote)
    : bucket(bucket),
      client_state_checker(
          base::MakeRefCounted<IndexedDBClientStateCheckerWrapper>(
              std::move(client_state_checker_remote))) {}

IndexedDBDispatcherHost::ReceiverContext::ReceiverContext(
    IndexedDBDispatcherHost::ReceiverContext&&) noexcept = default;

IndexedDBDispatcherHost::ReceiverContext::~ReceiverContext() = default;

void IndexedDBDispatcherHost::AddReceiver(
    IndexedDBDispatcherHost::ReceiverContext context,
    mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver) {
  DCHECK(IDBTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(pending_receiver), std::move(context));
}

void IndexedDBDispatcherHost::AddDatabaseBinding(
    std::unique_ptr<blink::mojom::IDBDatabase> database,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabase>
        pending_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  database_receivers_.Add(std::move(database), std::move(pending_receiver));
}

mojo::PendingAssociatedRemote<blink::mojom::IDBCursor>
IndexedDBDispatcherHost::CreateCursorBinding(
    const storage::BucketLocator& bucket_locator,
    std::unique_ptr<IndexedDBCursor> cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto cursor_impl = std::make_unique<CursorImpl>(
      std::move(cursor), bucket_locator, this, IDBTaskRunner());
  auto* cursor_impl_ptr = cursor_impl.get();
  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> remote;
  mojo::ReceiverId receiver_id = cursor_receivers_.Add(
      std::move(cursor_impl), remote.InitWithNewEndpointAndPassReceiver());
  cursor_impl_ptr->OnRemoveBinding(
      base::BindOnce(&IndexedDBDispatcherHost::RemoveCursorBinding,
                     weak_factory_.GetWeakPtr(), receiver_id));
  return remote;
}

void IndexedDBDispatcherHost::RemoveCursorBinding(
    mojo::ReceiverId receiver_id) {
  cursor_receivers_.Remove(receiver_id);
}

void IndexedDBDispatcherHost::AddTransactionBinding(
    std::unique_ptr<blink::mojom::IDBTransaction> transaction,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transaction_receivers_.Add(std::move(transaction), std::move(receiver));
}

storage::mojom::BlobStorageContext*
IndexedDBDispatcherHost::mojo_blob_storage_context() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return indexed_db_context_->blob_storage_context();
}

storage::mojom::FileSystemAccessContext*
IndexedDBDispatcherHost::file_system_access_context() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return indexed_db_context_->file_system_access_context();
}

void IndexedDBDispatcherHost::GetDatabaseInfo(
    GetDatabaseInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!receivers_.current_context().bucket.has_value()) {
    std::move(callback).Run(
        {}, blink::mojom::IDBError::New(
                blink::mojom::IDBException::kUnknownError, u"Internal error."));
    return;
  }

  const auto& bucket = *receivers_.current_context().bucket;
  storage::BucketLocator bucket_locator = bucket.ToBucketLocator();
  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket_locator);
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      bucket_locator, indexed_db_path, std::move(callback));
}

void IndexedDBDispatcherHost::Open(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
        database_callbacks_remote,
    const std::u16string& name,
    int64_t version,
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!receivers_.current_context().bucket.has_value()) {
    auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
        this->AsWeakPtr(), absl::nullopt, std::move(pending_callbacks),
        IDBTaskRunner());
    IndexedDBDatabaseError error = IndexedDBDatabaseError(
        blink::mojom::IDBException::kUnknownError, u"Internal error.");
    callbacks->OnError(error);
    return;
  }

  const auto& bucket = *receivers_.current_context().bucket;
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      this->AsWeakPtr(), bucket, std::move(pending_callbacks), IDBTaskRunner());
  auto database_callbacks = base::MakeRefCounted<IndexedDBDatabaseCallbacks>(
      indexed_db_context_, std::move(database_callbacks_remote),
      IDBTaskRunner());
  storage::BucketLocator bucket_locator = bucket.ToBucketLocator();
  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket_locator);

  auto create_transaction_callback = base::BindOnce(
      &IndexedDBDispatcherHost::CreateAndBindTransactionImpl, AsWeakPtr(),
      std::move(transaction_receiver), bucket_locator);
  std::unique_ptr<IndexedDBPendingConnection> connection =
      std::make_unique<IndexedDBPendingConnection>(
          std::move(callbacks), std::move(database_callbacks), transaction_id,
          version, std::move(create_transaction_callback));

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  indexed_db_context_->GetIDBFactory()->Open(
      name, std::move(connection), bucket_locator, indexed_db_path,
      receivers_.current_context().client_state_checker);
}

void IndexedDBDispatcherHost::DeleteDatabase(
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    const std::u16string& name,
    bool force_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Return error if failed to retrieve bucket from the QuotaManager.
  if (!receivers_.current_context().bucket.has_value()) {
    auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
        this->AsWeakPtr(), absl::nullopt, std::move(pending_callbacks),
        IDBTaskRunner());
    IndexedDBDatabaseError error = IndexedDBDatabaseError(
        blink::mojom::IDBException::kUnknownError, u"Internal error.");
    callbacks->OnError(error);
    return;
  }

  const auto& bucket = *receivers_.current_context().bucket;
  auto callbacks = base::MakeRefCounted<IndexedDBCallbacks>(
      this->AsWeakPtr(), bucket, std::move(pending_callbacks), IDBTaskRunner());

  storage::BucketLocator bucket_locator = bucket.ToBucketLocator();
  base::FilePath indexed_db_path =
      indexed_db_context_->GetDataPath(bucket_locator);
  indexed_db_context_->GetIDBFactory()->DeleteDatabase(
      name, std::move(callbacks), bucket_locator, indexed_db_path, force_close);
}

void IndexedDBDispatcherHost::CreateAndBindTransactionImpl(
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    const storage::BucketLocator& bucket_locator,
    base::WeakPtr<IndexedDBTransaction> transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto transaction_impl = std::make_unique<TransactionImpl>(
      transaction, bucket_locator, this->AsWeakPtr(), IDBTaskRunner());
  AddTransactionBinding(std::move(transaction_impl),
                        std::move(transaction_receiver));
}

void IndexedDBDispatcherHost::BindFileReader(
    const base::FilePath& path,
    base::Time expected_modification_time,
    base::RepeatingClosure release_callback,
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
      this, path, expected_modification_time, std::move(release_callback),
      file_task_runner_, io_task_runner_, std::move(receiver));
  file_reader_map_.insert({path, std::move(reader)});
}

void IndexedDBDispatcherHost::RemoveBoundReaders(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_reader_map_.erase(path);
}

void IndexedDBDispatcherHost::CreateAllExternalObjects(
    const storage::BucketLocator& bucket_locator,
    const std::vector<IndexedDBExternalObject>& objects,
    std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("IndexedDB",
               "IndexedDBDispatcherHost::CreateAllExternalObjects");

  DCHECK_EQ(objects.size(), mojo_objects->size());
  if (objects.empty())
    return;

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

        mojo_blob_storage_context()->RegisterFromDataItem(
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
          DCHECK(!blob_info.file_system_access_token().empty());
          file_system_access_context()->DeserializeHandle(
              bucket_locator.storage_key, blob_info.file_system_access_token(),
              mojo_token.InitWithNewPipeAndPassReceiver());
        }
        mojo_object->get_file_system_access_token() = std::move(mojo_token);
        break;
      }
    }
  }
}

base::SequencedTaskRunner* IndexedDBDispatcherHost::IDBTaskRunner() const {
  return indexed_db_context_->IDBTaskRunner();
}

}  // namespace content
