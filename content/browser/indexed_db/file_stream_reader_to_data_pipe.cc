// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"

#include <optional>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content::indexed_db {

namespace {

// TODO(estade): rename this class and this file.
class FileStreamReaderToDataPipe {
 public:
  FileStreamReaderToDataPipe(
      const base::FilePath& file_path,
      uint64_t expected_file_size,
      uint64_t offset,
      uint64_t read_length,
      mojo::ScopedDataPipeProducerHandle dest,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client);
  FileStreamReaderToDataPipe(const base::FilePath& file_path,
                             uint64_t offset,
                             uint64_t read_length,
                             mojo::ScopedDataPipeProducerHandle dest,
                             base::OnceCallback<void(int)> completion_callback);

  ~FileStreamReaderToDataPipe();

  void Start();

 private:
  void ReadMore();

  void OnDataPipeWritable(MojoResult result);
  void OnDataPipeClosed(MojoResult result);
  void OnComplete(net::Error result);

  base::File file_;
  mojo::ScopedDataPipeProducerHandle dest_;

  // Exactly one of these two members will be non-null.
  mojo::Remote<blink::mojom::BlobReaderClient> client_;
  base::OnceCallback<void(int)> completion_callback_;

  uint64_t transferred_bytes_ = 0;
  uint64_t offset_;
  uint64_t read_length_ = 0;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  // Optional so that its construction can be deferred.
  std::optional<mojo::SimpleWatcher> writable_handle_watcher_;
};

FileStreamReaderToDataPipe::FileStreamReaderToDataPipe(
    const base::FilePath& file_path,
    uint64_t expected_file_size,
    uint64_t offset,
    uint64_t read_length,
    mojo::ScopedDataPipeProducerHandle dest,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client)
    : dest_(std::move(dest)), client_(std::move(client)), offset_(offset) {
  read_length_ = std::min(expected_file_size, read_length);
  client_->OnCalculatedSize(expected_file_size, read_length_);
  file_.Initialize(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

FileStreamReaderToDataPipe::FileStreamReaderToDataPipe(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t read_length,
    mojo::ScopedDataPipeProducerHandle dest,
    base::OnceCallback<void(int)> completion_callback)
    : dest_(std::move(dest)),
      completion_callback_(std::move(completion_callback)),
      offset_(offset),
      read_length_(read_length) {
  file_.Initialize(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

FileStreamReaderToDataPipe::~FileStreamReaderToDataPipe() = default;

void FileStreamReaderToDataPipe::Start() {
  if (read_length_ == 0) {
    OnComplete(net::OK);
    return;
  }

  if (!file_.IsValid()) {
    OnComplete(net::FileErrorToNetError(file_.error_details()));
    return;
  }

  writable_handle_watcher_.emplace(FROM_HERE,
                                   mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  writable_handle_watcher_->Watch(
      dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&FileStreamReaderToDataPipe::OnDataPipeWritable,
                          base::Unretained(this)));

  ReadMore();
}

void FileStreamReaderToDataPipe::ReadMore() {
  // This loop shouldn't block the thread for *too* long as the mojo pipe has a
  // capacity of 2MB (i.e. `BeginWrite()` will return MOJO_RESULT_SHOULD_WAIT at
  // some point when reading in a very large file).
  while (true) {
    DCHECK(!pending_write_);
    MojoResult mojo_result =
        network::NetToMojoPendingBuffer::BeginWrite(&dest_, &pending_write_);
    switch (mojo_result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_SHOULD_WAIT:
        // The pipe is full.  We need to wait for it to have more space.
        writable_handle_watcher_->ArmOrNotify();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // The data pipe consumer handle has been closed.
        OnComplete(net::ERR_ABORTED);
        return;
      default:
        // The body stream is in a bad state. Bail out.
        OnComplete(net::ERR_UNEXPECTED);
        return;
    }

    size_t read_bytes = base::checked_cast<size_t>(
        std::min(static_cast<uint64_t>(pending_write_->size()),
                 read_length_ - transferred_bytes_));
    base::span<uint8_t> buffer =
        base::as_writable_byte_span(*pending_write_).first(read_bytes);
    std::optional<size_t> result =
        file_.Read(offset_ + transferred_bytes_, buffer);

    if (!result) {
      // Read error.
      dest_ = pending_write_->Complete(0);
      OnComplete(net::ERR_FAILED);
      return;
    }

    dest_ = pending_write_->Complete(*result);
    transferred_bytes_ += *result;

    // The file read may receive less than `read_length_` in some environments,
    // causing `ReadMore()` to reach the end of the file.  When this happens,
    // report success with `transferred_bytes_` in the data pipe.  See
    // crbug.com/383157185 for more details.
    const bool end_of_file = (*result == 0);

    if (transferred_bytes_ >= read_length_ || end_of_file) {
      OnComplete(net::OK);
      return;
    }

    pending_write_ = nullptr;
  }
}

void FileStreamReaderToDataPipe::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void FileStreamReaderToDataPipe::OnComplete(net::Error result) {
  // Resets the watchers, pipes and the exchange handler, so that
  // we will never be called back.
  if (writable_handle_watcher_) {
    writable_handle_watcher_->Cancel();
  }
  pending_write_ = nullptr;
  dest_.reset();

  if (client_) {
    client_->OnComplete(result, transferred_bytes_);
  } else {
    std::move(completion_callback_).Run(result);
  }
  // `this` is only used by on-disk backing stores.
  LogNetError("IndexedDB.BackingStore.ReadBlob", /*in_memory=*/false, result);
  delete this;
}

}  // namespace

void OpenFileAndReadIntoPipe(
    const base::FilePath& file_path,
    uint64_t expected_file_size,
    uint64_t offset,
    uint64_t read_length,
    mojo::ScopedDataPipeProducerHandle dest,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  (new FileStreamReaderToDataPipe(file_path, expected_file_size, offset,
                                  read_length, std::move(dest),
                                  std::move(client)))
      ->Start();
}

void OpenFileAndReadIntoPipe(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t read_length,
    mojo::ScopedDataPipeProducerHandle dest,
    base::OnceCallback<void(int)> completion_callback) {
  (new FileStreamReaderToDataPipe(file_path, offset, read_length,
                                  std::move(dest),
                                  std::move(completion_callback)))
      ->Start();
}

}  // namespace content::indexed_db
