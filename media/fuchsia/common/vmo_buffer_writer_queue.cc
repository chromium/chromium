// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/fuchsia/common/vmo_buffer_writer_queue.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process_metrics.h"
#include "media/base/decoder_buffer.h"

namespace media {

VmoBufferWriterQueue::PendingBuffer::PendingBuffer(
    scoped_refptr<DecoderBuffer> buffer)
    : buffer(buffer) {
  DCHECK(buffer);
}

VmoBufferWriterQueue::PendingBuffer::~PendingBuffer() = default;

VmoBufferWriterQueue::PendingBuffer::PendingBuffer(PendingBuffer&& other) =
    default;

const uint8_t* VmoBufferWriterQueue::PendingBuffer::data() const {
  return buffer->data() + buffer_pos;
}

size_t VmoBufferWriterQueue::PendingBuffer::bytes_left() const {
  return buffer->size() - buffer_pos;
}

void VmoBufferWriterQueue::PendingBuffer::AdvanceCurrentPos(size_t bytes) {
  DCHECK_LE(bytes, bytes_left());
  buffer_pos += bytes;
}

VmoBufferWriterQueue::VmoBufferWriterQueue() {
  DETACH_FROM_THREAD(thread_checker_);
}

VmoBufferWriterQueue::~VmoBufferWriterQueue() = default;

void VmoBufferWriterQueue::EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_buffers_.emplace_back(std::move(buffer));
  PumpPackets();
}

void VmoBufferWriterQueue::Start(std::vector<VmoBuffer> buffers,
                                 SendPacketCB send_packet_cb,
                                 EndOfStreamCB end_of_stream_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffers_.empty());
  DCHECK(!buffers.empty());

  buffers_ = std::move(buffers);
  send_packet_cb_ = std::move(send_packet_cb);
  end_of_stream_cb_ = std::move(end_of_stream_cb);

  // Initialize |unused_buffers_|.
  unused_buffers_.reserve(buffers_.size());
  for (size_t i = 0; i < buffers_.size(); ++i) {
    unused_buffers_.push_back(i);
  }

  PumpPackets();
}

bool VmoBufferWriterQueue::IsBlocked() const {
  return unused_buffers_.empty();
}

void VmoBufferWriterQueue::PumpPackets() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto weak_this = weak_factory_.GetWeakPtr();

  while (!buffers_.empty() && !is_paused_ &&
         input_queue_position_ < pending_buffers_.size()) {
    PendingBuffer* current_buffer = &pending_buffers_[input_queue_position_];

    if (current_buffer->buffer->end_of_stream()) {
      // Pop the EndOfStream buffer if it's the only buffer. Otherwise, mark it
      // as complete so that ReleaseBuffer will pop it.
      if (input_queue_position_ == 0) {
        pending_buffers_.pop_front();
        DCHECK(pending_buffers_.empty());
      } else {
        current_buffer->is_complete = true;
        input_queue_position_ += 1;
      }
      end_of_stream_cb_.Run();
      if (!weak_this)
        return;
      continue;
    }

    if (unused_buffers_.empty()) {
      // No input buffer available.
      return;
    }

    size_t buffer_index = unused_buffers_.back();
    unused_buffers_.pop_back();

    size_t bytes_filled = buffers_[buffer_index].Write(
        base::make_span(current_buffer->data(), current_buffer->bytes_left()));
    current_buffer->AdvanceCurrentPos(bytes_filled);

    bool buffer_end = current_buffer->bytes_left() == 0;

    StreamProcessorHelper::IoPacket packet(
        buffer_index, /*offset=*/0, bytes_filled,
        current_buffer->buffer->timestamp(), buffer_end, /*key_frame=*/false,
        base::BindOnce(&VmoBufferWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), buffer_index));

    if (buffer_end) {
      current_buffer->tail_sysmem_buffer_index = buffer_index;
      input_queue_position_ += 1;
    }

    send_packet_cb_.Run(current_buffer->buffer.get(), std::move(packet));
    if (!weak_this)
      return;
  }
}

void VmoBufferWriterQueue::ResetQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_buffers_.clear();
  input_queue_position_ = 0;
  is_paused_ = false;
}

void VmoBufferWriterQueue::ResetBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  buffers_.clear();
  send_packet_cb_ = SendPacketCB();
  end_of_stream_cb_ = EndOfStreamCB();

  // Invalidate weak pointers, so ReleaseBuffer() is not called for the old
  // buffers.
  weak_factory_.InvalidateWeakPtrs();
}

void VmoBufferWriterQueue::ResetPositionAndPause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& buffer : pending_buffers_) {
    buffer.buffer_pos = 0;
    buffer.is_complete = false;

    // All packets that were pending will need to be resent. Reset
    // |tail_sysmem_buffer_index| to ensure that these packets are not removed
    // from the queue in ReleaseBuffer().
    buffer.tail_sysmem_buffer_index = std::nullopt;
  }
  input_queue_position_ = 0;
  is_paused_ = true;
}

void VmoBufferWriterQueue::Unpause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_paused_);
  is_paused_ = false;
  PumpPackets();
}

void VmoBufferWriterQueue::ReleaseBuffer(size_t buffer_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!buffers_.empty());

  // Mark the input buffer as complete.
  for (size_t i = 0; i < input_queue_position_; ++i) {
    if (pending_buffers_[i].tail_sysmem_buffer_index == buffer_index)
      pending_buffers_[i].is_complete = true;
  }

  // Remove all complete buffers from the head of the queue since we no longer
  // need them. Note that currently StreamProcessor doesn't guarantee that input
  // buffers are released in the same order they were sent (see
  // https://fuchsia.googlesource.com/fuchsia/+/3b12c8c5/sdk/fidl/fuchsia.media/stream_processor.fidl#1646
  // ). This means that some complete buffers will need to stay in the queue
  // until all preceding packets are released as well.
  while (!pending_buffers_.empty() && pending_buffers_.front().is_complete) {
    pending_buffers_.pop_front();
    DCHECK_GT(input_queue_position_, 0U);
    input_queue_position_--;
  }

  unused_buffers_.push_back(buffer_index);
  PumpPackets();
}

size_t VmoBufferWriterQueue::num_buffers() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return buffers_.size();
}

}  // namespace media
