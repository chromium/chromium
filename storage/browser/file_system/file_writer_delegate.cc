// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_writer_delegate.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

static const int kReadBufSize = 32768;

FileWriterDelegate::FileWriterDelegate(
    std::unique_ptr<FileStreamWriter> file_stream_writer,
    FlushPolicy flush_policy)
    : file_stream_writer_(std::move(file_stream_writer)),
      writing_started_(false),
      flush_policy_(flush_policy),
      bytes_written_backlog_(0),
      bytes_written_(0),
      bytes_read_(0),
      io_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kReadBufSize)),
      data_pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
}

FileWriterDelegate::~FileWriterDelegate() = default;

void FileWriterDelegate::Start(std::unique_ptr<BlobReader> blob_reader,
                               DelegateWriteCallback write_callback) {
  write_callback_ = std::move(write_callback);

  if (!blob_reader) {
    OnReadError(base::File::FILE_ERROR_FAILED);
    return;
  }

  blob_reader_ = std::move(blob_reader);
  BlobReader::Status status = blob_reader_->CalculateSize(base::BindOnce(
      &FileWriterDelegate::OnDidCalculateSize, weak_factory_.GetWeakPtr()));
  switch (status) {
    case BlobReader::Status::NET_ERROR:
      OnDidCalculateSize(blob_reader_->net_error());
      return;
    case BlobReader::Status::DONE:
      OnDidCalculateSize(net::OK);
      return;
    case BlobReader::Status::IO_PENDING:
      // Do nothing.
      return;
  }
  NOTREACHED();
}

void FileWriterDelegate::Start(mojo::ScopedDataPipeConsumerHandle data_pipe,
                               DelegateWriteCallback write_callback) {
  write_callback_ = std::move(write_callback);

  if (!data_pipe) {
    OnReadError(base::File::FILE_ERROR_FAILED);
    return;
  }

  data_pipe_ = std::move(data_pipe);
  data_pipe_watcher_.Watch(
      data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&FileWriterDelegate::OnDataPipeReady,
                          weak_factory_.GetWeakPtr()));
  data_pipe_watcher_.ArmOrNotify();
}

void FileWriterDelegate::Cancel() {
  // Destroy the reader and invalidate weak ptrs to prevent pending callbacks.
  blob_reader_ = nullptr;
  data_pipe_watcher_.Cancel();
  data_pipe_.reset();
  weak_factory_.InvalidateWeakPtrs();

  const int status = file_stream_writer_->Cancel(base::BindOnce(
      &FileWriterDelegate::OnWriteCancelled, weak_factory_.GetWeakPtr()));
  // Return true to finish immediately if we have no pending writes.
  // Otherwise we'll do the final cleanup in the Cancel callback.
  if (status != net::ERR_IO_PENDING) {
    write_callback_.Run(base::File::FILE_ERROR_ABORT, 0,
                        GetCompletionStatusOnError());
  }
}

void FileWriterDelegate::OnDidCalculateSize(int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    OnReadError(NetErrorToFileError(net_error));
    return;
  }
  Read();
}

void FileWriterDelegate::OnReadCompleted(int bytes_read) {
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);

  if (bytes_read < 0) {
    OnReadError(NetErrorToFileError(bytes_read));
    return;
  }
  OnDataReceived(bytes_read);
}

void FileWriterDelegate::Read() {
  bytes_written_ = 0;
  if (blob_reader_) {
    BlobReader::Status status =
        blob_reader_->Read(io_buffer_.get(), io_buffer_->size(), &bytes_read_,
                           base::BindOnce(&FileWriterDelegate::OnReadCompleted,
                                          weak_factory_.GetWeakPtr()));
    switch (status) {
      case BlobReader::Status::NET_ERROR:
        OnReadCompleted(blob_reader_->net_error());
        return;
      case BlobReader::Status::DONE:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&FileWriterDelegate::OnReadCompleted,
                                      weak_factory_.GetWeakPtr(), bytes_read_));
        return;
      case BlobReader::Status::IO_PENDING:
        // Do nothing.
        return;
    }
    NOTREACHED();
  }

  DCHECK(data_pipe_);
  size_t num_bytes = 0;
  MojoResult result = data_pipe_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                           io_buffer_->span(), num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    data_pipe_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_OK) {
    bytes_read_ = base::checked_cast<int>(num_bytes);
    OnReadCompleted(bytes_read_);
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // Pipe closed, done reading.
    OnReadCompleted(0);
    return;
  }
  // Some unknown error, this shouldn't happen.
  NOTREACHED();
}

void FileWriterDelegate::OnDataReceived(int bytes_read) {
  bytes_read_ = bytes_read;
  if (bytes_read == 0) {  // We're done.
    OnProgress(0, true);
  } else {
    // This could easily be optimized to rotate between a pool of buffers, so
    // that we could read and write at the same time.  It's not yet clear that
    // it's necessary.
    cursor_ =
        base::MakeRefCounted<net::DrainableIOBuffer>(io_buffer_, bytes_read_);
    Write();
  }
}

void FileWriterDelegate::Write() {
  writing_started_ = true;
  int64_t bytes_to_write = bytes_read_ - bytes_written_;
  int write_response = file_stream_writer_->Write(
      cursor_.get(), static_cast<int>(bytes_to_write),
      base::BindOnce(&FileWriterDelegate::OnDataWritten,
                     weak_factory_.GetWeakPtr()));
  if (write_response > 0) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FileWriterDelegate::OnDataWritten,
                                  weak_factory_.GetWeakPtr(), write_response));
  } else if (net::ERR_IO_PENDING != write_response) {
    OnWriteError(NetErrorToFileError(write_response));
  } else {
    async_write_in_progress_ = true;
  }
}

void FileWriterDelegate::OnDataWritten(int write_response) {
  async_write_in_progress_ = false;
  if (saved_read_error_ != base::File::FILE_OK) {
    OnReadError(saved_read_error_);
    return;
  }

  if (write_response > 0) {
    OnProgress(write_response, false);
    cursor_->DidConsume(write_response);
    bytes_written_ += write_response;
    if (bytes_written_ == bytes_read_)
      Read();
    else
      Write();
  } else {
    OnWriteError(NetErrorToFileError(write_response));
  }
}

FileWriterDelegate::WriteProgressStatus
FileWriterDelegate::GetCompletionStatusOnError() const {
  return writing_started_ ? ERROR_WRITE_STARTED : ERROR_WRITE_NOT_STARTED;
}

void FileWriterDelegate::OnReadError(base::File::Error error) {
  if (async_write_in_progress_) {
    // Error signaled by the URLRequest while writing. This will be processed
    // when the write completes.
    saved_read_error_ = error;
    return;
  }

  // Destroy the reader and invalidate weak ptrs to prevent pending callbacks.
  blob_reader_.reset();
  data_pipe_watcher_.Cancel();
  data_pipe_.reset();
  weak_factory_.InvalidateWeakPtrs();

  if (writing_started_)
    MaybeFlushForCompletion(error, 0, ERROR_WRITE_STARTED);
  else
    write_callback_.Run(error, 0, ERROR_WRITE_NOT_STARTED);
}

void FileWriterDelegate::OnWriteError(base::File::Error error) {
  // Destroy the reader and invalidate weak ptrs to prevent pending callbacks.
  blob_reader_.reset();
  data_pipe_watcher_.Cancel();
  data_pipe_.reset();
  weak_factory_.InvalidateWeakPtrs();

  // Errors when writing are not recoverable, so don't bother flushing.
  write_callback_.Run(
      error, 0,
      writing_started_ ? ERROR_WRITE_STARTED : ERROR_WRITE_NOT_STARTED);
}

void FileWriterDelegate::OnProgress(int bytes_written, bool done) {
  DCHECK(bytes_written + bytes_written_backlog_ >= bytes_written_backlog_);
  static const int kMinProgressDelayMS = 200;
  base::Time currentTime = base::Time::Now();
  if (done || last_progress_event_time_.is_null() ||
      (currentTime - last_progress_event_time_).InMilliseconds() >
          kMinProgressDelayMS) {
    bytes_written += bytes_written_backlog_;
    last_progress_event_time_ = currentTime;
    bytes_written_backlog_ = 0;

    if (done) {
      MaybeFlushForCompletion(base::File::FILE_OK, bytes_written,
                              SUCCESS_COMPLETED);
    } else {
      write_callback_.Run(base::File::FILE_OK, bytes_written,
                          SUCCESS_IO_PENDING);
    }
    return;
  }
  bytes_written_backlog_ += bytes_written;
}

void FileWriterDelegate::OnWriteCancelled(int status) {
  write_callback_.Run(base::File::FILE_ERROR_ABORT, 0,
                      GetCompletionStatusOnError());
}

void FileWriterDelegate::MaybeFlushForCompletion(
    base::File::Error error,
    int bytes_written,
    WriteProgressStatus progress_status) {
  if (flush_policy_ == FlushPolicy::NO_FLUSH_ON_COMPLETION) {
    write_callback_.Run(error, bytes_written, progress_status);
    return;
  }
  // DCHECK_EQ on enum classes is not supported.
  DCHECK(flush_policy_ == FlushPolicy::FLUSH_ON_COMPLETION);

  int flush_error = file_stream_writer_->Flush(
      FlushMode::kEndOfFile,
      base::BindOnce(&FileWriterDelegate::OnFlushed, weak_factory_.GetWeakPtr(),
                     error, bytes_written, progress_status));
  if (flush_error != net::ERR_IO_PENDING)
    OnFlushed(error, bytes_written, progress_status, flush_error);
}

void FileWriterDelegate::OnFlushed(base::File::Error error,
                                   int bytes_written,
                                   WriteProgressStatus progress_status,
                                   int flush_error) {
  if (error == base::File::FILE_OK && flush_error != net::OK) {
    // If the Flush introduced an error, overwrite the status.
    // Otherwise, keep the original error status.
    error = NetErrorToFileError(flush_error);
    progress_status = GetCompletionStatusOnError();
  }
  write_callback_.Run(error, bytes_written, progress_status);
}

void FileWriterDelegate::OnDataPipeReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  Read();
}

}  // namespace storage
