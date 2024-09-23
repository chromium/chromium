// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_pipe_read_write_util.h"

using media::mojo_pipe_read_write_util::IsPipeReadWriteError;

namespace media {

// Creates mojo::DataPipe and sets `producer_handle` and `consumer_handle`.
// Returns true on success. Otherwise returns false and reset the handles.
bool CreateDataPipe(uint32_t capacity,
                    mojo::ScopedDataPipeProducerHandle* producer_handle,
                    mojo::ScopedDataPipeConsumerHandle* consumer_handle) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = capacity;

  auto result =
      mojo::CreateDataPipe(&options, *producer_handle, *consumer_handle);

  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "DataPipe creation failed with " << result;
    producer_handle->reset();
    consumer_handle->reset();
    return false;
  }

  return true;
}

uint32_t GetDefaultDecoderBufferConverterCapacity(DemuxerStream::Type type) {
  uint32_t capacity = 0;

  if (type == DemuxerStream::AUDIO) {
    // TODO(timav): Consider capacity calculation based on AudioDecoderConfig.
    capacity = 512 * 1024;
  } else if (type == DemuxerStream::VIDEO) {
    // Video can get quite large; at 4K, VP9 delivers packets which are ~1MB in
    // size; so allow for some head room.
    // TODO(xhwang, sandersd): Provide a better way to customize this value.
    capacity = 2 * (1024 * 1024);
  } else {
    NOTREACHED_IN_MIGRATION() << "Unsupported type: " << type;
    // Choose an arbitrary size.
    capacity = 512 * 1024;
  }

  return capacity;
}

// MojoDecoderBufferReader

// static
std::unique_ptr<MojoDecoderBufferReader> MojoDecoderBufferReader::Create(
    uint32_t capacity,
    mojo::ScopedDataPipeProducerHandle* producer_handle) {
  DVLOG(1) << __func__;
  DCHECK_GT(capacity, 0u);

  // Create a MojoDecoderBufferReader even on the failure case and
  // `ReadDecoderBuffer()` below will fail.
  // TODO(xhwang): Update callers to handle failure so we can return null.
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  std::ignore = CreateDataPipe(capacity, producer_handle, &consumer_handle);
  return std::make_unique<MojoDecoderBufferReader>(std::move(consumer_handle));
}

MojoDecoderBufferReader::MojoDecoderBufferReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()),
      armed_(false),
      bytes_read_(0) {
  DVLOG(1) << __func__;

  if (!consumer_handle_.is_valid()) {
    DLOG(ERROR) << __func__ << ": Invalid consumer handle";
    return;
  }

  MojoResult result = pipe_watcher_.Watch(
      consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&MojoDecoderBufferReader::OnPipeReadable,
                          base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << __func__
                << ": Failed to start watching the pipe. result=" << result;
    consumer_handle_.reset();
  }
}

MojoDecoderBufferReader::~MojoDecoderBufferReader() {
  DVLOG(1) << __func__;
  CancelAllPendingReadCBs();
  if (flush_cb_)
    std::move(flush_cb_).Run();
}

void MojoDecoderBufferReader::ReadDecoderBuffer(
    mojom::DecoderBufferPtr mojo_buffer,
    ReadCB read_cb) {
  DVLOG(3) << __func__;
  DCHECK(!flush_cb_);

  if (!consumer_handle_.is_valid()) {
    DCHECK(pending_read_cbs_.empty());
    CancelReadCB(std::move(read_cb));
    return;
  }

  scoped_refptr<DecoderBuffer> media_buffer(
      mojo_buffer.To<scoped_refptr<DecoderBuffer>>());
  DCHECK(media_buffer);

  if (MediaTraceIsEnabled() && !media_buffer->end_of_stream()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "media,gpu", "MojoDecoderBufferReader::Read",
        media_buffer->timestamp().InMicroseconds());
    read_cb = base::BindOnce(
        [](ReadCB read_cb, scoped_refptr<DecoderBuffer> buffer) {
          TRACE_EVENT_NESTABLE_ASYNC_END2(
              "media,gpu", "MojoDecoderBufferReader::Read",
              buffer->timestamp().InMicroseconds(), "timestamp",
              buffer->timestamp().InMicroseconds(), "read_bytes",
              buffer->size());
          std::move(read_cb).Run(std::move(buffer));
        },
        std::move(read_cb));
  }
  // We don't want reads to complete out of order, so we queue them even if they
  // are zero-sized.
  pending_read_cbs_.push_back(std::move(read_cb));
  pending_buffers_.push_back(std::move(media_buffer));

  // Do nothing if a read is already scheduled.
  if (armed_)
    return;

  // To reduce latency, always process pending reads immediately.
  ProcessPendingReads();
}

void MojoDecoderBufferReader::Flush(base::OnceClosure flush_cb) {
  DVLOG(2) << __func__;
  DCHECK(!flush_cb_);

  if (pending_read_cbs_.empty()) {
    std::move(flush_cb).Run();
    return;
  }

  flush_cb_ = std::move(flush_cb);
}

bool MojoDecoderBufferReader::HasPendingReads() const {
  return !pending_read_cbs_.empty();
}

void MojoDecoderBufferReader::CancelReadCB(ReadCB read_cb) {
  DVLOG(1) << "Failed to read DecoderBuffer because the pipe is already closed";
  std::move(read_cb).Run(nullptr);
}

void MojoDecoderBufferReader::CancelAllPendingReadCBs() {
  while (!pending_read_cbs_.empty()) {
    ReadCB read_cb = std::move(pending_read_cbs_.front());
    pending_read_cbs_.pop_front();
    // TODO(sandersd): Make sure there are no possible re-entrancy issues
    // here. Perhaps these should be posted, or merged into a single error
    // callback?
    CancelReadCB(std::move(read_cb));
  }
}

void MojoDecoderBufferReader::CompleteCurrentRead() {
  DVLOG(4) << __func__;
  DCHECK(!pending_read_cbs_.empty());
  DCHECK_EQ(pending_read_cbs_.size(), pending_buffers_.size());

  ReadCB read_cb = std::move(pending_read_cbs_.front());
  pending_read_cbs_.pop_front();

  scoped_refptr<DecoderBuffer> buffer = std::move(pending_buffers_.front());
  pending_buffers_.pop_front();

  DCHECK(buffer->end_of_stream() || buffer->size() == bytes_read_);
  bytes_read_ = 0;

  std::move(read_cb).Run(std::move(buffer));

  if (pending_read_cbs_.empty() && flush_cb_) {
    std::move(flush_cb_).Run();
  }
}

void MojoDecoderBufferReader::ScheduleNextRead() {
  DVLOG(4) << __func__;
  DCHECK(!armed_);
  DCHECK(!pending_buffers_.empty());

  armed_ = true;
  pipe_watcher_.ArmOrNotify();
}

void MojoDecoderBufferReader::OnPipeReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DVLOG(4) << __func__ << "(" << result << ", " << state.readable() << ")";

  // |MOJO_RESULT_CANCELLED| may be dispatched even while the SimpleWatcher
  // is disarmed, and no further notifications will be dispatched after that.
  DCHECK(armed_ || result == MOJO_RESULT_CANCELLED);

  armed_ = false;

  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK(state.readable());
  ProcessPendingReads();
}

void MojoDecoderBufferReader::ProcessPendingReads() {
  DVLOG(4) << __func__;
  DCHECK(!armed_);
  DCHECK(!pending_buffers_.empty());

  while (!pending_buffers_.empty()) {
    DecoderBuffer* buffer = pending_buffers_.front().get();

    size_t buffer_size = 0u;
    if (!pending_buffers_.front()->end_of_stream()) {
      buffer_size = buffer->size();
    }

    // Immediately complete empty reads.
    // A non-EOS buffer can have zero size. See http://crbug.com/663438
    if (buffer_size == 0) {
      // TODO(sandersd): Make sure there are no possible re-entrancy issues
      // here. Perhaps read callbacks should be posted?
      CompleteCurrentRead();
      continue;
    }

    size_t actually_read_bytes = 0;
    MojoResult result = consumer_handle_->ReadData(
        MOJO_WRITE_DATA_FLAG_NONE,
        // We may be starting to read a new buffer (|bytes_read_| == 0), or
        // recovering from a previous partial read (|bytes_read_| > 0).
        buffer->writable_span().subspan(bytes_read_), actually_read_bytes);

    if (IsPipeReadWriteError(result)) {
      OnPipeError(result);
      return;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      ScheduleNextRead();
      return;
    }

    DCHECK_EQ(result, MOJO_RESULT_OK);
    DVLOG(4) << __func__ << ": " << actually_read_bytes << " bytes read.";
    DCHECK_GT(actually_read_bytes, 0u);
    bytes_read_ += actually_read_bytes;

    // TODO(sandersd): Make sure there are no possible re-entrancy issues
    // here.
    if (bytes_read_ == buffer_size) {
      CompleteCurrentRead();
    }

    // Since we can still read, try to read more.
  }
}

void MojoDecoderBufferReader::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  consumer_handle_.reset();

  if (!pending_buffers_.empty()) {
    DVLOG(1) << __func__ << ": reading from data pipe failed. result=" << result
             << ", buffer size=" << pending_buffers_.front()->size()
             << ", num_bytes(read)=" << bytes_read_;
    bytes_read_ = 0;
    pending_buffers_.clear();
    CancelAllPendingReadCBs();
  }
}

// MojoDecoderBufferWriter

// static
std::unique_ptr<MojoDecoderBufferWriter> MojoDecoderBufferWriter::Create(
    uint32_t capacity,
    mojo::ScopedDataPipeConsumerHandle* consumer_handle) {
  DVLOG(1) << __func__;
  DCHECK_GT(capacity, 0u);

  // Create a MojoDecoderBufferWriter even on the failure case and
  // `WriteDecoderBuffer()` below will fail.
  // TODO(xhwang): Update callers to handle failure so we can return null.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  std::ignore = CreateDataPipe(capacity, &producer_handle, consumer_handle);
  return std::make_unique<MojoDecoderBufferWriter>(std::move(producer_handle));
}

MojoDecoderBufferWriter::MojoDecoderBufferWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : producer_handle_(std::move(producer_handle)),
      pipe_watcher_(FROM_HERE,
                    mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                    base::SequencedTaskRunner::GetCurrentDefault()),
      armed_(false),
      bytes_written_(0) {
  DVLOG(1) << __func__;

  if (!producer_handle_.is_valid()) {
    DLOG(ERROR) << __func__ << ": Invalid producer handle";
    return;
  }

  MojoResult result = pipe_watcher_.Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&MojoDecoderBufferWriter::OnPipeWritable,
                          base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << __func__
                << ": Failed to start watching the pipe. result=" << result;
    producer_handle_.reset();
  }
}

MojoDecoderBufferWriter::~MojoDecoderBufferWriter() {
  DVLOG(1) << __func__;
}

void MojoDecoderBufferWriter::ScheduleNextWrite() {
  DVLOG(4) << __func__;
  DCHECK(!armed_);
  DCHECK(!pending_buffers_.empty());

  armed_ = true;
  pipe_watcher_.ArmOrNotify();
}

mojom::DecoderBufferPtr MojoDecoderBufferWriter::WriteDecoderBuffer(
    scoped_refptr<DecoderBuffer> media_buffer) {
  DVLOG(3) << __func__;

  // DecoderBuffer cannot be written if the pipe is already closed.
  if (!producer_handle_.is_valid()) {
    DVLOG(1)
        << __func__
        << ": Failed to write DecoderBuffer because the pipe is already closed";
    return nullptr;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojom::DecoderBuffer::From(*media_buffer);

  // A non-EOS buffer can have zero size. See http://crbug.com/663438
  if (media_buffer->end_of_stream() || media_buffer->empty()) {
    return mojo_buffer;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu",
                                    "MojoDecoderBufferWriter::Write",
                                    media_buffer->timestamp().InMicroseconds());
  // Queue writing the buffer's data into our DataPipe.
  pending_buffers_.push_back(std::move(media_buffer));

  // Do nothing if a write is already scheduled. Otherwise, to reduce latency,
  // always try to write data to the pipe first.
  if (!armed_)
    ProcessPendingWrites();

  return mojo_buffer;
}

void MojoDecoderBufferWriter::OnPipeWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DVLOG(4) << __func__ << "(" << result << ", " << state.writable() << ")";

  // |MOJO_RESULT_CANCELLED| may be dispatched even while the SimpleWatcher
  // is disarmed, and no further notifications will be dispatched after that.
  DCHECK(armed_ || result == MOJO_RESULT_CANCELLED);

  armed_ = false;

  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK(state.writable());
  ProcessPendingWrites();
}

void MojoDecoderBufferWriter::ProcessPendingWrites() {
  DVLOG(4) << __func__;
  DCHECK(!armed_);
  DCHECK(!pending_buffers_.empty());

  while (!pending_buffers_.empty()) {
    DecoderBuffer* buffer = pending_buffers_.front().get();

    base::span<const uint8_t> bytes_to_write(*buffer);
    DCHECK_GT(bytes_to_write.size(), 0u) << "Unexpected EOS or empty buffer";

    // We may be starting to write a new buffer (|bytes_written_| == 0), or
    // recovering from a previous partial write (|bytes_written_| > 0).
    bytes_to_write = bytes_to_write.subspan(bytes_written_);
    DCHECK_GT(bytes_to_write.size(), 0u);

    size_t actually_written_bytes = 0;
    MojoResult result = producer_handle_->WriteData(
        bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);

    if (IsPipeReadWriteError(result)) {
      OnPipeError(result);
      return;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      ScheduleNextWrite();
      return;
    }

    DCHECK_EQ(MOJO_RESULT_OK, result);
    DVLOG(4) << __func__ << ": " << actually_written_bytes << " bytes written.";
    DCHECK_GT(actually_written_bytes, 0u);
    bytes_written_ += actually_written_bytes;
    if (actually_written_bytes == bytes_to_write.size()) {
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "media,gpu", "MojoDecoderBufferWriter::Write",
          buffer->timestamp().InMicroseconds(), "timestamp",
          buffer->timestamp().InMicroseconds(), "write_bytes", bytes_written_);
      pending_buffers_.pop_front();
      bytes_written_ = 0;
    }

    // Since we can still write, try to write more.
  }
}

void MojoDecoderBufferWriter::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  producer_handle_.reset();

  if (!pending_buffers_.empty()) {
    DVLOG(1) << __func__ << ": writing to data pipe failed. result=" << result
             << ", buffer size=" << pending_buffers_.front()->size()
             << ", num_bytes(written)=" << bytes_written_;
    if (MediaTraceIsEnabled()) {
      for (const auto& buffer : pending_buffers_) {
        TRACE_EVENT_NESTABLE_ASYNC_END2(
            "media,gpu", "MojoDecoderBufferWriter::Write",
            buffer->timestamp().InMicroseconds(), "timestamp",
            buffer->timestamp().InMicroseconds(), "write_bytes",
            bytes_written_);
      }
    }
    pending_buffers_.clear();
    bytes_written_ = 0;
  }
}

}  // namespace media
