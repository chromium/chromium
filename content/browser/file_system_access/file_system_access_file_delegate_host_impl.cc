// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"

#include <cstdint>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/big_io_buffer.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

namespace {

void ReadOnIOThread(scoped_refptr<storage::FileSystemContext> context,
                    storage::FileSystemURL url,
                    int64_t offset,
                    scoped_refptr<storage::BigIOBuffer> buffer,
                    scoped_refptr<base::SequencedTaskRunner> reply_runner,
                    base::OnceCallback<void(int)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto wrapped_callback =
      base::BindPostTask(std::move(reply_runner), std::move(callback));

  // The reader must be constructed on the IO thread.
  std::unique_ptr<storage::FileStreamReader> reader =
      context->CreateFileStreamReader(
          url, offset, /*max_bytes_to_read=*/buffer->size(), base::Time());

  // Create two copies of `wrapped_callback` though it can still only be
  // called at most once. This is safe because Read() is guaranteed not to
  // call `wrapped_callback` if it returns synchronously.
  auto split_callback = base::SplitOnceCallback(std::move(wrapped_callback));
  // Keep the reader alive by binding it to its callback.
  storage::FileStreamReader* reader_ptr = reader.get();
  int rv = reader_ptr->Read(
      buffer.get(), buffer->size(),
      base::BindOnce(
          [](std::unique_ptr<storage::FileStreamReader>,
             base::OnceCallback<void(int)> callback,
             int bytes_read) { std::move(callback).Run(bytes_read); },
          std::move(reader), std::move(split_callback.first)));
  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

}  // namespace

FileSystemAccessFileDelegateHostImpl::FileSystemAccessFileDelegateHostImpl(
    FileSystemAccessManagerImpl* manager,
    const storage::FileSystemURL& url,
    base::PassKey<FileSystemAccessAccessHandleHostImpl> /*pass_key*/,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
        receiver)
    : manager_(manager), url_(url), receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(manager_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&FileSystemAccessFileDelegateHostImpl::OnDisconnect,
                     base::Unretained(this)));
}

FileSystemAccessFileDelegateHostImpl::~FileSystemAccessFileDelegateHostImpl() =
    default;

struct FileSystemAccessFileDelegateHostImpl::WriteState {
  WriteCallback callback;
  int64_t bytes_written = 0;
};

void FileSystemAccessFileDelegateHostImpl::Read(int64_t offset,
                                                int bytes_to_read,
                                                ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (offset < 0) {
    receiver_.ReportBadMessage("SyncAccesHandle with a negative read offset.");
    return;
  }
  if (bytes_to_read < 0) {
    receiver_.ReportBadMessage(
        "SyncAccesHandle trying to read a negative number of bytes.");
    return;
  }

  // FileStreamReader::Read takes an int. Do not allocate more memory than
  // Chrome will be allowed to contiguously allocate at once.
  int max_bytes_to_read =
      std::min(bytes_to_read,
               base::saturated_cast<int>(partition_alloc::MaxDirectMapped()));

  auto buffer = base::MakeRefCounted<storage::BigIOBuffer>(max_bytes_to_read);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReadOnIOThread, base::WrapRefCounted(file_system_context()), url(),
          offset, buffer, base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&FileSystemAccessFileDelegateHostImpl::DidRead,
                         weak_factory_.GetWeakPtr(), buffer,
                         std::move(callback))));
}

void FileSystemAccessFileDelegateHostImpl::DidRead(
    scoped_refptr<storage::BigIOBuffer> buffer,
    ReadCallback callback,
    int rv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (rv < 0) {
    std::move(callback).Run(std::optional<mojo_base::BigBuffer>(),
                            storage::NetErrorToFileError(rv),
                            /*bytes_read=*/0);
    return;
  }

  int bytes_read = rv;
  mojo_base::BigBuffer result = buffer->TakeBuffer();
  // BigBuffers must be completely filled before transfer to avoid leaking
  // information across processes.
  if (static_cast<size_t>(bytes_read) < result.size()) {
    size_t bytes_to_fill = result.size() - static_cast<size_t>(bytes_read);
    memset(result.data() + bytes_read, 0, bytes_to_fill);
  }

  std::move(callback).Run(std::move(result), base::File::Error::FILE_OK,
                          bytes_read);
}

void FileSystemAccessFileDelegateHostImpl::Write(
    int64_t offset,
    mojo::ScopedDataPipeConsumerHandle data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (offset < 0) {
    receiver_.ReportBadMessage("SyncAccesHandle with a negative write offset.");
    return;
  }

  manager()->DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::WriteStream,
      base::BindRepeating(&FileSystemAccessFileDelegateHostImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})),
      url(), std::move(data), base::checked_cast<int64_t>(offset));
}

void FileSystemAccessFileDelegateHostImpl::DidWrite(WriteState* state,
                                                    base::File::Error result,
                                                    int64_t bytes,
                                                    bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    // This cast is guaranteed to be safe because the data buffer we're writing
    // from cannot be more than int bytes.
    int bytes_written = base::checked_cast<int>(state->bytes_written);
    std::move(state->callback).Run(result, bytes_written);
  }
}

void FileSystemAccessFileDelegateHostImpl::GetLength(
    GetLengthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::GetMetadata,
      base::BindOnce(
          [](GetLengthCallback callback, base::File::Error file_error,
             const base::File::Info& file_info) {
            if (file_error == base::File::Error::FILE_OK) {
              std::move(callback).Run(file_error, file_info.size);
              return;
            }
            std::move(callback).Run(file_error, 0);
          },
          std::move(callback)),
      url(),
      storage::FileSystemOperation::GetMetadataFieldSet(
          {storage::FileSystemOperation::GetMetadataField::kSize}));
}

void FileSystemAccessFileDelegateHostImpl::SetLength(
    int64_t length,
    SetLengthCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (length < 0) {
    receiver_.ReportBadMessage(
        "SyncAccesHandle with a negative truncate length.");
    return;
  }

  manager()->DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::Truncate,
      std::move(callback), url(), length);
}

void FileSystemAccessFileDelegateHostImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
}

}  // namespace content
