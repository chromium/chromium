// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_delegate_host_impl.h"

#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

namespace {

void ReadOnIOThread(
    storage::FileStreamReader* reader,
    scoped_refptr<FileSystemAccessFileDelegateHostImpl::BigIOBuffer> buffer,
    scoped_refptr<base::SequencedTaskRunner> reply_runner,
    base::OnceCallback<void(int)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto wrapped_callback =
      base::BindPostTask(std::move(reply_runner), std::move(callback));

  // Create two copies of |wrapped_callback| though it can still only be
  // called at most once. This is safe because Read() is guaranteed not to
  // call |wrapped_callback| if it returns synchronously.
  auto split_callback = base::SplitOnceCallback(std::move(wrapped_callback));
  int rv = reader->Read(buffer.get(), buffer->size(),
                        base::BindOnce(std::move(split_callback.first)));
  if (rv != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(rv);
}

}  // namespace

FileSystemAccessFileDelegateHostImpl::BigIOBuffer::BigIOBuffer(size_t size)
    : net::IOBufferWithSize(nullptr, size) {
  buffer_ = mojo_base::BigBuffer(size);
  data_ = reinterpret_cast<char*>(buffer_.data());
  DCHECK(data_);
}

FileSystemAccessFileDelegateHostImpl::BigIOBuffer::~BigIOBuffer() {
  // Must clear `data_` so base class doesn't attempt to delete[] a pointer
  // it doesn't own.
  this->data_ = nullptr;
  this->size_ = 0UL;
}

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
  uint64_t bytes_written = 0;
};

void FileSystemAccessFileDelegateHostImpl::Read(uint64_t offset,
                                                uint64_t bytes_to_read,
                                                ReadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // FileStreamReader::Read takes an int. Do not allocate more memory than
  // Chrome will be allowed to contiguously allocate at once.
  int max_bytes_to_read =
      std::min(base::saturated_cast<int>(bytes_to_read),
               base::saturated_cast<int>(base::MaxDirectMapped()));

  auto buffer = base::MakeRefCounted<BigIOBuffer>(max_bytes_to_read);

  std::unique_ptr<storage::FileStreamReader> reader =
      file_system_context()->CreateFileStreamReader(
          url(), offset, /*max_bytes_to_read=*/buffer->size(), base::Time());

  storage::FileStreamReader* reader_ptr = reader.get();
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReadOnIOThread, reader_ptr, buffer,
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(&FileSystemAccessFileDelegateHostImpl::DidRead,
                         weak_factory_.GetWeakPtr(), std::move(reader), buffer,
                         std::move(callback))));
}

void FileSystemAccessFileDelegateHostImpl::DidRead(
    std::unique_ptr<storage::FileStreamReader> reader,
    scoped_refptr<BigIOBuffer> buffer,
    ReadCallback callback,
    int rv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (rv < 0) {
    std::move(callback).Run(absl::optional<mojo_base::BigBuffer>(),
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
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DoFileSystemOperation(
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

void FileSystemAccessFileDelegateHostImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
}

}  // namespace content
