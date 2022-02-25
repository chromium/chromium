// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_incognito_file_delegate.h"

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

// Creates a mojo data pipe, where the capacity of the data pipe is derived from
// the provided `data_size`. Returns false if creating the data pipe failed.
bool CreateDataPipeForSize(uint64_t data_size,
                           mojo::ScopedDataPipeProducerHandle& producer,
                           mojo::ScopedDataPipeConsumerHandle& consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = BlobUtils::GetDataPipeCapacity(data_size);

  MojoResult rv = CreateDataPipe(&options, producer, consumer);
  if (rv != MOJO_RESULT_OK) {
    return false;
  }
  return true;
}

void WriteDataToProducer(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    scoped_refptr<base::RefCountedData<Vector<uint8_t>>> data) {
  DCHECK(!IsMainThread())
      << "WriteDataToProducer must not be called on the main thread";

  auto data_source = std::make_unique<mojo::StringDataSource>(
      base::span<const char>(reinterpret_cast<const char*>(data->data.data()),
                             data->data.size()),
      mojo::StringDataSource::AsyncWritingMode::
          STRING_STAYS_VALID_UNTIL_COMPLETION);

  auto producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* producer_raw = producer.get();
  // Bind the producer and data to the callback to ensure they stay alive for
  // the duration of the write.
  producer_raw->Write(
      std::move(data_source),
      WTF::Bind([](std::unique_ptr<mojo::DataPipeProducer>,
                   scoped_refptr<base::RefCountedData<Vector<uint8_t>>>,
                   MojoResult) {},
                std::move(producer), std::move(data)));
}

}  // namespace

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::CreateForIncognito(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
        incognito_file_remote) {
  return MakeGarbageCollected<FileSystemAccessIncognitoFileDelegate>(
      context, std::move(incognito_file_remote),
      base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessIncognitoFileDelegate::FileSystemAccessIncognitoFileDelegate(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
        incognito_file_remote,
    base::PassKey<FileSystemAccessFileDelegate>)
    : mojo_ptr_(context),
      task_runner_(context->GetTaskRunner(TaskType::kMiscPlatformAPI)) {
  mojo_ptr_.Bind(std::move(incognito_file_remote), task_runner_);
  DCHECK(mojo_ptr_.is_bound());
}

void FileSystemAccessIncognitoFileDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemAccessFileDelegate::Trace(visitor);
}

base::FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  base::File::Error file_error;
  int bytes_read;
  absl::optional<mojo_base::BigBuffer> buffer;
  mojo_ptr_->Read(offset, data.size(), &buffer, &file_error, &bytes_read);

  CHECK_EQ(buffer.has_value(), file_error == base::File::FILE_OK);

  if (buffer.has_value()) {
    CHECK_LE(static_cast<uint64_t>(bytes_read), data.size());
    CHECK_LE(buffer->size(), data.size());

    memcpy(data.data(), buffer->data(), buffer->size());
  } else {
    CHECK_EQ(bytes_read, 0);
  }

  return file_error == base::File::Error::FILE_OK ? bytes_read : file_error;
}

base::FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (!CreateDataPipeForSize(data.size(), producer_handle, consumer_handle)) {
    return base::File::Error::FILE_ERROR_FAILED;
  }

  auto ref_counted_data =
      base::MakeRefCounted<base::RefCountedData<Vector<uint8_t>>>();
  ref_counted_data->data.Append(data.data(),
                                static_cast<wtf_size_t>(data.size()));

  // Write the data to the data pipe on another thread. This is safe to run in
  // parallel to the `Write()` call, since the browser can read from the pipe as
  // data is written. The `Write()` call won't complete until the mojo datapipe
  // has closed, so we must write to the data pipe on anther thread to be able
  // to close the pipe when all data has been written.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&WriteDataToProducer, std::move(producer_handle),
                          ref_counted_data));

  base::File::Error file_error;
  int bytes_written;
  mojo_ptr_->Write(offset, std::move(consumer_handle), &file_error,
                   &bytes_written);

  return file_error == base::File::Error::FILE_OK ? bytes_written : file_error;
}

void FileSystemAccessIncognitoFileDelegate::GetLength(
    base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojo_ptr_->GetLength(WTF::Bind(
      [](base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback,
         base::File::Error file_error, uint64_t length) {
        std::move(callback).Run(
            file_error == base::File::Error::FILE_OK ? length : file_error);
      },
      std::move(callback)));
}

void FileSystemAccessIncognitoFileDelegate::SetLength(
    int64_t length,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (length < 0) {
    // This method is expected to finish asynchronously, so post a task to the
    // current sequence to return the error.
    task_runner_->PostTask(
        FROM_HERE, WTF::Bind(std::move(callback),
                             base::File::Error::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  mojo_ptr_->SetLength(length, WTF::Bind(std::move(callback)));
}

void FileSystemAccessIncognitoFileDelegate::Flush(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Flush is a no-op for in-memory file systems. Even if the file delegate is
  // used for other FS types, writes through the FileSystemOperationRunner are
  // automatically flushed. If this proves to be too slow, we can consider
  // changing the FileSystemAccessFileDelegateHostImpl to write with a
  // FileStreamWriter and only flushing when this method is called.
  task_runner_->PostTask(FROM_HERE,
                         WTF::Bind(std::move(callback), /*success=*/true));
}

void FileSystemAccessIncognitoFileDelegate::Close(base::OnceClosure callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  mojo_ptr_.reset();

  task_runner_->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace blink
