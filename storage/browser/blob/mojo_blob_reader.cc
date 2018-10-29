// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/mojo_blob_reader.h"

#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/net_adapters.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/blob/blob_utils.h"

namespace storage {

// static
void MojoBlobReader::Create(const BlobDataHandle* handle,
                            const net::HttpByteRange& range,
                            std::unique_ptr<Delegate> delegate) {
  new MojoBlobReader(handle, range, std::move(delegate));
}

MojoBlobReader::MojoBlobReader(const BlobDataHandle* handle,
                               const net::HttpByteRange& range,
                               std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      byte_range_(range),
      blob_reader_(handle->CreateReader()),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      peer_closed_handle_watcher_(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                                  base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobReader", this, "uuid", handle->uuid());
  DCHECK(delegate_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoBlobReader::Start, weak_factory_.GetWeakPtr()));
}

MojoBlobReader::~MojoBlobReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_ASYNC_END1("Blob", "BlobReader", this, "bytes_written",
                         total_written_bytes_);
}

void MojoBlobReader::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (blob_reader_->net_error()) {
    NotifyCompletedAndDeleteIfNeeded(blob_reader_->net_error());
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("Blob", "BlobReader::CountSize", this);
  BlobReader::Status size_status = blob_reader_->CalculateSize(base::BindOnce(
      &MojoBlobReader::DidCalculateSize, base::Unretained(this)));
  switch (size_status) {
    case BlobReader::Status::NET_ERROR:
      TRACE_EVENT_ASYNC_END1("Blob", "BlobReader::CountSize", this, "result",
                             "error");
      NotifyCompletedAndDeleteIfNeeded(blob_reader_->net_error());
      return;
    case BlobReader::Status::IO_PENDING:
      return;
    case BlobReader::Status::DONE:
      DidCalculateSize(net::OK);
      return;
  }

  NOTREACHED();
}

void MojoBlobReader::NotifyCompletedAndDeleteIfNeeded(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blob_reader_ = nullptr;
  if (!notified_completed_) {
    delegate_->OnComplete(static_cast<net::Error>(result),
                          total_written_bytes_);
    notified_completed_ = true;
  }

  bool has_data_pipe = pending_write_ || response_body_stream_.is_valid();
  if (!has_data_pipe)
    delete this;
}

void MojoBlobReader::DidCalculateSize(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != net::OK) {
    TRACE_EVENT_ASYNC_END1("Blob", "BlobReader::CountSize", this, "result",
                           "error");
    NotifyCompletedAndDeleteIfNeeded(result);
    return;
  }

  TRACE_EVENT_ASYNC_END2("Blob", "BlobReader::CountSize", this, "result",
                         "success", "size", blob_reader_->total_size());

  // Apply the range requirement.
  if (!byte_range_.ComputeBounds(blob_reader_->total_size())) {
    NotifyCompletedAndDeleteIfNeeded(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  DCHECK_LE(byte_range_.first_byte_position(),
            byte_range_.last_byte_position() + 1);
  uint64_t length = base::checked_cast<uint64_t>(
      byte_range_.last_byte_position() - byte_range_.first_byte_position() + 1);

  if (blob_reader_->SetReadRange(byte_range_.first_byte_position(), length) !=
      BlobReader::Status::DONE) {
    NotifyCompletedAndDeleteIfNeeded(blob_reader_->net_error());
    return;
  }

  if (delegate_->DidCalculateSize(blob_reader_->total_size(),
                                  blob_reader_->remaining_bytes()) ==
      Delegate::REQUEST_SIDE_DATA) {
    if (!blob_reader_->has_side_data()) {
      DidReadSideData(BlobReader::Status::DONE);
    } else {
      BlobReader::Status read_status =
          blob_reader_->ReadSideData(base::BindOnce(
              &MojoBlobReader::DidReadSideData, base::Unretained(this)));
      if (read_status != BlobReader::Status::IO_PENDING)
        DidReadSideData(BlobReader::Status::DONE);
    }
  } else {
    StartReading();
  }
}

void MojoBlobReader::DidReadSideData(BlobReader::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != BlobReader::Status::DONE) {
    NotifyCompletedAndDeleteIfNeeded(blob_reader_->net_error());
    return;
  }
  delegate_->DidReadSideData(blob_reader_->side_data());
  StartReading();
}

void MojoBlobReader::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!response_body_stream_);

  response_body_stream_ = delegate_->PassDataPipe();
  peer_closed_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&MojoBlobReader::OnResponseBodyStreamClosed,
                          base::Unretained(this)));
  peer_closed_handle_watcher_.ArmOrNotify();

  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&MojoBlobReader::OnResponseBodyStreamReady,
                          base::Unretained(this)));

  // Start reading...
  ReadMore();
}

void MojoBlobReader::ReadMore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!pending_write_.get());
  DCHECK(response_body_stream_);

  uint32_t num_bytes = 0;
  // TODO: we should use the abstractions in MojoAsyncResourceHandler.
  MojoResult result = network::NetToMojoPendingBuffer::BeginWrite(
      &response_body_stream_, &pending_write_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  } else if (result != MOJO_RESULT_OK) {
    // The response body stream is in a bad state. Bail.
    writable_handle_watcher_.Cancel();
    response_body_stream_.reset();
    NotifyCompletedAndDeleteIfNeeded(net::ERR_UNEXPECTED);
    return;
  }

  num_bytes = std::min(num_bytes, blink::BlobUtils::GetDataPipeChunkSize());

  TRACE_EVENT_ASYNC_BEGIN0("Blob", "BlobReader::ReadMore", this);
  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
  DCHECK(pending_write_);
  auto buf =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_write_.get());
  int bytes_read = 0;
  BlobReader::Status read_status = blob_reader_->Read(
      buf.get(), static_cast<int>(num_bytes), &bytes_read,
      base::BindOnce(&MojoBlobReader::DidRead, base::Unretained(this), false));
  switch (read_status) {
    case BlobReader::Status::NET_ERROR:
      DidRead(true, blob_reader_->net_error());
      return;
    case BlobReader::Status::IO_PENDING:
      // Wait for DidRead.
      return;
    case BlobReader::Status::DONE:
      DidRead(true, bytes_read);
      return;
  }
}

void MojoBlobReader::DidRead(bool completed_synchronously, int num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (num_bytes < 0) {
    TRACE_EVENT_ASYNC_END2("Blob", "BlobReader::ReadMore", this, "result",
                           "error", "net_error", num_bytes);
    writable_handle_watcher_.Cancel();
    pending_write_->Complete(0);
    pending_write_ = nullptr;  // This closes the data pipe.
    NotifyCompletedAndDeleteIfNeeded(num_bytes);
    return;
  }
  if (num_bytes > 0)
    delegate_->DidRead(num_bytes);
  TRACE_EVENT_ASYNC_END2("Blob", "BlobReader::ReadMore", this, "result",
                         "success", "num_bytes", num_bytes);
  response_body_stream_ = pending_write_->Complete(num_bytes);
  total_written_bytes_ += num_bytes;
  pending_write_ = nullptr;
  if (num_bytes == 0) {
    response_body_stream_.reset();  // This closes the data pipe.
    NotifyCompletedAndDeleteIfNeeded(net::OK);
    return;
  }
  if (completed_synchronously) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoBlobReader::ReadMore, weak_factory_.GetWeakPtr()));
  } else {
    ReadMore();
  }
}

void MojoBlobReader::OnResponseBodyStreamClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_body_stream_.reset();
  pending_write_ = nullptr;
  NotifyCompletedAndDeleteIfNeeded(net::ERR_ABORTED);
}

void MojoBlobReader::OnResponseBodyStreamReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    OnResponseBodyStreamClosed(MOJO_RESULT_OK, state);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK);
  ReadMore();
}

}  // namespace storage
