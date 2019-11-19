// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_buffer_writer_queue.h"

#include <zircon/rights.h>
#include <algorithm>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/process/process_metrics.h"
#include "media/base/decoder_buffer.h"

namespace media {

struct SysmemBufferWriterQueue::PendingBuffer {
  PendingBuffer(scoped_refptr<DecoderBuffer> buffer) : buffer(buffer) {
    DCHECK(buffer);
  }
  ~PendingBuffer() = default;

  PendingBuffer(PendingBuffer&& other) = default;
  PendingBuffer& operator=(PendingBuffer&& other) = default;

  const uint8_t* data() const { return buffer->data() + buffer_pos; }
  size_t bytes_left() const { return buffer->data_size() - buffer_pos; }
  void AdvanceCurrentPos(size_t bytes) {
    DCHECK_LE(bytes, bytes_left());
    buffer_pos += bytes;
  }

  scoped_refptr<DecoderBuffer> buffer;
  size_t buffer_pos = 0;

  // Set to true when the consumer has finished processing the buffer and it can
  // be released.
  bool is_complete = false;

  // Index of the last buffer in the sysmem buffer collection that was used for
  // this input buffer. Valid only when |bytes_left()==0|.
  size_t tail_sysmem_buffer_index = 0;
};

SysmemBufferWriterQueue::SysmemBufferWriterQueue() = default;
SysmemBufferWriterQueue::~SysmemBufferWriterQueue() = default;

void SysmemBufferWriterQueue::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_buffers_.push_back(PendingBuffer(buffer));
  PumpPackets();
}

void SysmemBufferWriterQueue::Start(std::unique_ptr<SysmemBufferWriter> writer,
                                    SendPacketCB send_packet_cb,
                                    EndOfStreamCB end_of_stream_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!writer_);

  writer_ = std::move(writer);
  send_packet_cb_ = std::move(send_packet_cb);
  end_of_stream_cb_ = std::move(end_of_stream_cb);

  PumpPackets();
}

void SysmemBufferWriterQueue::PumpPackets() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto weak_this = weak_factory_.GetWeakPtr();

  while (writer_ && !is_paused_ &&
         input_queue_position_ < pending_buffers_.size()) {
    PendingBuffer* current_buffer = &pending_buffers_[input_queue_position_];

    if (current_buffer->buffer->end_of_stream()) {
      pending_buffers_.pop_front();
      end_of_stream_cb_.Run();
      if (!weak_this)
        return;
      continue;
    }

    base::Optional<size_t> index_opt = writer_->Acquire();

    if (!index_opt.has_value()) {
      // No input buffer available.
      return;
    }

    size_t sysmem_buffer_index = index_opt.value();

    size_t bytes_filled = writer_->Write(
        sysmem_buffer_index,
        base::make_span(current_buffer->data(), current_buffer->bytes_left()));
    current_buffer->AdvanceCurrentPos(bytes_filled);

    bool buffer_end = current_buffer->bytes_left() == 0;

    auto packet = StreamProcessorHelper::IoPacket::CreateInput(
        sysmem_buffer_index, bytes_filled, current_buffer->buffer->timestamp(),
        buffer_end,
        base::BindOnce(&SysmemBufferWriterQueue::ReleaseBuffer,
                       weak_factory_.GetWeakPtr(), sysmem_buffer_index));

    if (buffer_end) {
      current_buffer->tail_sysmem_buffer_index = sysmem_buffer_index;
      input_queue_position_ += 1;
    }

    send_packet_cb_.Run(current_buffer->buffer.get(), std::move(packet));
    if (!weak_this)
      return;
  }
}

void SysmemBufferWriterQueue::ResetQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  pending_buffers_.clear();
  input_queue_position_ = 0;
  is_paused_ = false;
}

void SysmemBufferWriterQueue::ResetBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  writer_.reset();
  send_packet_cb_ = SendPacketCB();
  end_of_stream_cb_ = EndOfStreamCB();

  // Invalidate weak pointers, so ReleaseBuffer() is not called for the old
  // buffers.
  weak_factory_.InvalidateWeakPtrs();
}

void SysmemBufferWriterQueue::ResetPositionAndPause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& buffer : pending_buffers_) {
    buffer.buffer_pos = 0;
    buffer.is_complete = false;
  }
  input_queue_position_ = 0;
  is_paused_ = true;
}

void SysmemBufferWriterQueue::Unpause() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_paused_);
  is_paused_ = false;
  PumpPackets();
}

void SysmemBufferWriterQueue::ReleaseBuffer(size_t buffer_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(writer_);

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

  writer_->Release(buffer_index);
  PumpPackets();
}

size_t SysmemBufferWriterQueue::num_buffers() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return writer_ ? writer_->num_buffers() : 0;
}

}  // namespace media
