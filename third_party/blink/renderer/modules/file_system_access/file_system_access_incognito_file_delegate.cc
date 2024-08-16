// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_incognito_file_delegate.h"

#include <optional>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
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
      WTF::BindOnce([](std::unique_ptr<mojo::DataPipeProducer>,
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
      write_helper_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({})) {
  mojo_ptr_.Bind(std::move(incognito_file_remote),
                 context->GetTaskRunner(TaskType::kStorage));
  DCHECK(mojo_ptr_.is_bound());
}

void FileSystemAccessIncognitoFileDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_ptr_);
  FileSystemAccessFileDelegate::Trace(visitor);
}

base::FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(offset, 0);

  base::File::Error file_error;
  int bytes_read;
  std::optional<mojo_base::BigBuffer> buffer;
  int bytes_to_read = base::saturated_cast<int>(data.size());
  mojo_ptr_->Read(offset, bytes_to_read, &buffer, &file_error, &bytes_read);

  CHECK_EQ(buffer.has_value(), file_error == base::File::FILE_OK);

  if (buffer.has_value()) {
    CHECK_LE(bytes_read, bytes_to_read);
    CHECK_LE(buffer->size(), static_cast<uint64_t>(bytes_to_read));

    memcpy(data.data(), buffer->data(), bytes_to_read);
  } else {
    CHECK_EQ(bytes_read, 0);
  }

  return file_error == base::File::Error::FILE_OK ? bytes_read : file_error;
}

base::FileErrorOr<int> FileSystemAccessIncognitoFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(offset, 0);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (!CreateDataPipeForSize(data.size(), producer_handle, consumer_handle)) {
    return base::unexpected(base::File::Error::FILE_ERROR_FAILED);
  }

  auto ref_counted_data =
      base::MakeRefCounted<base::RefCountedData<Vector<uint8_t>>>();
  ref_counted_data->data.AppendSpan(data);

  // Write the data to the data pipe on another thread. This is safe to run in
  // parallel to the `Write()` call, since the browser can read from the pipe as
  // data is written. The `Write()` call won't complete until the mojo datapipe
  // has closed, so we must write to the data pipe on anther thread to be able
  // to close the pipe when all data has been written.
  PostCrossThreadTask(
      *write_helper_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WriteDataToProducer, std::move(producer_handle),
                          ref_counted_data));

  base::File::Error file_error;
  int bytes_written;
  mojo_ptr_->Write(offset, std::move(consumer_handle), &file_error,
                   &bytes_written);

  return file_error == base::File::Error::FILE_OK ? bytes_written : file_error;
}

base::FileErrorOr<int64_t> FileSystemAccessIncognitoFileDelegate::GetLength() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::File::Error file_error;
  int64_t length;
  mojo_ptr_->GetLength(&file_error, &length);
  CHECK_GE(length, 0);
  return file_error == base::File::Error::FILE_OK ? length : file_error;
}

base::FileErrorOr<bool> FileSystemAccessIncognitoFileDelegate::SetLength(
    int64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(length, 0);
  base::File::Error file_error;
  mojo_ptr_->SetLength(length, &file_error);
  return file_error == base::File::Error::FILE_OK ? true : file_error;
}

bool FileSystemAccessIncognitoFileDelegate::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Flush is a no-op for in-memory file systems. Even if the file delegate is
  // used for other FS types, writes through the FileSystemOperationRunner are
  // automatically flushed. If this proves to be too slow, we can consider
  // changing the FileSystemAccessFileDelegateHostImpl to write with a
  // FileStreamWriter and only flushing when this method is called.
  return true;
}

void FileSystemAccessIncognitoFileDelegate::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo_ptr_.reset();
}

}  // namespace blink
