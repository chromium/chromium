// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_VMO_BUFFER_WRITER_QUEUE_H_
#define MEDIA_FUCHSIA_COMMON_VMO_BUFFER_WRITER_QUEUE_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>

#include <deque>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

class DecoderBuffer;

// A helper that keeps a queue of pending DecodeBuffers, writes them to a set of
// VmoBuffers and generates StreamProcessor packets.
class MEDIA_EXPORT VmoBufferWriterQueue {
 public:
  // Callback passed to StartSender(). |buffer| corresponds to the original
  // buffer from which the |packet| was generated.
  using SendPacketCB =
      base::RepeatingCallback<void(const DecoderBuffer* buffer,
                                   StreamProcessorHelper::IoPacket packet)>;

  // Called when processing DecoderBuffer that's marked as end-of-stream.
  using EndOfStreamCB = base::RepeatingClosure;

  VmoBufferWriterQueue();
  ~VmoBufferWriterQueue();

  VmoBufferWriterQueue(VmoBufferWriterQueue&) = delete;
  VmoBufferWriterQueue& operator=(VmoBufferWriterQueue&) = delete;

  // Enqueues buffer to the queue.
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);

  // Sets the buffers to use and starts sending outgoing packets using
  // |send_packet_cb|. |end_of_stream_cb| will be called when processing each
  // end-of-stream buffer.
  void Start(std::vector<VmoBuffer> buffers,
             SendPacketCB send_packet_cb,
             EndOfStreamCB end_of_stream_cb);

  // Resets all pending buffers. Keeps the underlying sysmem buffers.
  void ResetQueue();

  // Resets the buffers. Keeps the current pending buffers, so they will still
  // be sent once the new collection is allocated and passed to Start().
  void ResetBuffers();

  // Resets pending queue position to the start of the queue and pauses the
  // writer. All pending buffers will be resent when Unpause() is called.
  // This method is used to handle OnStreamFailed event received from
  // StreamProcessor, particularly to handle NoKey error in CDM. When that event
  // is received the StreamProcessor client should assumes that all queued
  // packets were not processed. Once the error condition is resolved (e.g. by
  // adding a new decryption key), the client should start a new stream and
  // resend all failed packets, which is achieved by calling Unpause()
  void ResetPositionAndPause();

  // Resumes sending packets on stream that was previously paused with
  // ResetPositionAndPause(). Should be called after starting a new stream in
  // the StreamProcessor (e.g. by calling StreamProcessorHelper::Reset()).
  void Unpause();

  // Number of buffers in the sysmem collection or 0 if sysmem buffers has not
  // been allocated (i.e. before Start()).
  size_t num_buffers() const;

  // Returns true of the queue is currently blocked, i.e. buffers passed
  // to EnqueueBuffer() will not be sent immediately.
  bool IsBlocked() const;

 private:
  struct PendingBuffer {
    PendingBuffer(scoped_refptr<DecoderBuffer> buffer);
    ~PendingBuffer();

    PendingBuffer(PendingBuffer&& other);
    PendingBuffer& operator=(PendingBuffer&& other) = default;

    const uint8_t* data() const;
    size_t bytes_left() const;
    void AdvanceCurrentPos(size_t bytes);

    scoped_refptr<DecoderBuffer> buffer;
    size_t buffer_pos = 0;

    // Set to true when the consumer has finished processing the buffer and it
    // can be released.
    bool is_complete = false;

    // Index of the last buffer in the sysmem buffer collection that was used to
    // send this input buffer. Should be set only when |bytes_left()==0|.
    std::optional<size_t> tail_sysmem_buffer_index;
  };

  class SysmemBuffer;

  // Pumps pending buffers to SendPacketCB.
  void PumpPackets();

  // Callback called when a packet is destroyed. It marks the buffer as unused
  // and tries to reuse it for other buffers if any.
  void ReleaseBuffer(size_t buffer_index);

  // Buffers that are waiting to be sent. A buffer is removed from the queue
  // when it and all previous buffers have finished decoding.
  std::deque<PendingBuffer> pending_buffers_;

  // Position of the current buffer in |pending_buffers_|.
  size_t input_queue_position_ = 0;

  // Indicates that the stream is paused and no packets should be sent until
  // Unpause() is called.
  bool is_paused_ = false;

  // Buffers for sysmem buffer collection. Empty until Start() is called.
  std::vector<VmoBuffer> buffers_;

  // Usd to store indices of the buffers that are not being used currently.
  std::vector<size_t> unused_buffers_;

  SendPacketCB send_packet_cb_;
  EndOfStreamCB end_of_stream_cb_;

  // FIDL interfaces are thread-affine (see crbug.com/1012875).
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VmoBufferWriterQueue> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_VMO_BUFFER_WRITER_QUEUE_H_
