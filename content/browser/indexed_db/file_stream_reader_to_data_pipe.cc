// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/file_stream_reader_to_data_pipe.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content::indexed_db {

namespace {

// This class owns itself and deletes itself after `completion_callback_` is
// run.
// TODO(estade): rename this class and this file.
class FileStreamReaderToDataPipe {
 public:
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
  void OnComplete(int result);

  base::File file_;
  mojo::ScopedDataPipeProducerHandle dest_;
  base::OnceCallback<void(int)> completion_callback_;
  uint64_t transferred_bytes_ = 0;
  uint64_t offset_;
  uint64_t read_length_;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  // Optional so that its construction can be deferred.
  std::optional<mojo::SimpleWatcher> writable_handle_watcher_;
};

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
  DCHECK(!writable_handle_watcher_.has_value());
  writable_handle_watcher_.emplace(FROM_HERE,
                                   mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  writable_handle_watcher_->Watch(
      dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&FileStreamReaderToDataPipe::OnDataPipeWritable,
                          base::Unretained(this)));

  file_.Initialize(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

FileStreamReaderToDataPipe::~FileStreamReaderToDataPipe() = default;

void FileStreamReaderToDataPipe::Start() {
  if (file_.IsValid()) {
    ReadMore();
  } else {
    OnComplete(net::FileErrorToNetError(file_.error_details()));
  }
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
        base::as_writable_bytes(base::make_span(*pending_write_))
            .first(read_bytes);
    std::optional<size_t> result =
        file_.Read(offset_ + transferred_bytes_, buffer);

    if (!result || !*result) {
      // Error or EOF.
      dest_ = pending_write_->Complete(0);
      OnComplete(net::ERR_FAILED);
      return;
    }

    dest_ = pending_write_->Complete(*result);
    transferred_bytes_ += *result;

    if (transferred_bytes_ >= read_length_) {
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

void FileStreamReaderToDataPipe::OnComplete(int result) {
  // Resets the watchers, pipes and the exchange handler, so that
  // we will never be called back.
  writable_handle_watcher_->Cancel();
  pending_write_ = nullptr;
  dest_.reset();

  std::move(completion_callback_).Run(result);
  delete this;
}

}  // namespace

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
