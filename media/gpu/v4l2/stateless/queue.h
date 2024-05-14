// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
#define MEDIA_GPU_V4L2_STATELESS_QUEUE_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/stateless/stateless_device.h"

namespace media {
using DequeueCB = base::OnceCallback<void(bool)>;

// V4L2 has two similar queues. Capitalized OUTPUT (for compressed frames)
// and CAPTURE (for uncompressed frames) are the designation that the V4L2
// framework uses. As these are counterintuitive for video decoding this class
// encapsulates the compressed frames into |InputQueue| and uncompressed frames
// into |OutputQueue|.
class MEDIA_GPU_EXPORT BaseQueue {
 public:
  BaseQueue(scoped_refptr<StatelessDevice> device,
            BufferType buffer_type,
            MemoryType memory_type);
  BaseQueue& operator=(const BaseQueue&);
  virtual ~BaseQueue() = 0;

  void DeallocateBuffers();
  bool StartStreaming();
  bool StopStreaming();
  bool BuffersAvailable() const { return free_buffer_indices_.size() > 0; }
  bool OutstandingBuffers() const {
    return free_buffer_indices_.size() < buffers_.size();
  }
  virtual const std::optional<Buffer> DequeueBuffer() = 0;

 protected:
  bool AllocateBuffers(uint32_t num_planes, size_t num_buffers);
  virtual std::string Description() = 0;
  void ReturnBuffer(uint32_t index);
  std::optional<uint32_t> GetFreeBufferIndex();

  scoped_refptr<StatelessDevice> device_;
  const BufferType buffer_type_;
  const MemoryType memory_type_;
  uint32_t num_planes_;
  std::vector<Buffer> buffers_;

  // Ordered set of free buffers. Because it is ordered the same index
  // will be used more often than if it was a ring buffer. Using a set
  // enforces the elements be unique.
  std::set<uint32_t> free_buffer_indices_;
};

class MEDIA_GPU_EXPORT InputQueue : public BaseQueue {
 public:
  static std::unique_ptr<InputQueue> Create(
      scoped_refptr<StatelessDevice> device,
      const VideoCodec codec);

  InputQueue(scoped_refptr<StatelessDevice> device, VideoCodec codec);

  // Pass all of the data necessary to decode a compressed frame off to the
  // driver.
  bool SubmitCompressedFrameData(const void* data,
                                 size_t length,
                                 uint64_t frame_id,
                                 const base::ScopedFD& request_fd);

  // Prepare the buffers required by the driver to hold the compressed data.
  bool PrepareBuffers(size_t num_buffers, const gfx::Size resolution);

  // Add buffers that have been dequeued into the list of buffers available
  // to be used again.
  const std::optional<Buffer> DequeueBuffer() override;

  // Check if the new resolution size would require larger buffers than those
  // already allocated.
  bool NeedToReallocateBuffers(const gfx::Size new_resolution);

 private:
  bool SetupFormat(const gfx::Size resolution);
  std::string Description() override;

  VideoCodec codec_;
  size_t allocated_buffer_size_ = 0;
};

class MEDIA_GPU_EXPORT OutputQueue : public BaseQueue {
 public:
  static std::unique_ptr<OutputQueue> Create(
      scoped_refptr<StatelessDevice> device);

  OutputQueue(scoped_refptr<StatelessDevice> device);
  ~OutputQueue() override;

  // Drivers can support multiple decode formats. ChromeOS would like to
  // use specific formats. |NegotiateFormat| chooses the decode format
  // that satisfies both requirements.
  bool NegotiateFormat();

  // Allocate and prepare the buffers that will store the decoded frames.
  bool PrepareBuffers(size_t num_buffers);

  // After a buffer has been used it needs to be returned to the pool of
  // available buffers. The client tracks using buffers using |frame_id|.
  void ReturnBuffer(uint64_t frame_id);

  // Enqueue the buffer into the driver.
  bool QueueBuffer();

  // Return the decoded frame format chosen by |NegotiateFormat|
  Fourcc GetQueueFormat() const { return buffer_format_.fourcc; }

  // Return the resolution of the decoded frames.
  gfx::Size GetVideoResolution() const { return buffer_format_.resolution; }

  // Record buffers that have finished decoded and have been dequeued so that
  // they can later be referenced.
  const std::optional<Buffer> DequeueBuffer() override;

  // Retrieve a |FrameResource| by |frame_id| that has already been decoded and
  // dequeued. Returns |nullptr| if there isn't a corresponding frame that has
  // been dequeued yet.
  scoped_refptr<FrameResource> GetFrame(uint64_t frame_id);

 private:
  // Create |FrameResource| by exporting the dmabuf backing the buffer.
  scoped_refptr<FrameResource> CreateFrame(const Buffer& buffer);

  std::string Description() override;

  BufferFormat buffer_format_;

  // Vector to hold |FrameResource|s for the life of the queue.
  std::vector<scoped_refptr<FrameResource>> frames_;

  // A mapping from frame id to buffer buffer index. Once a frame is decoded it
  // placed in this map. The frame id to buffer index mapping is how the input
  // queue is mapped to the output queue.
  std::map<uint64_t, uint32_t> decoded_and_dequeued_frames_;
};

// The Request API closely ties the input and output queues together.
class MEDIA_GPU_EXPORT RequestQueue {
 public:
  static std::unique_ptr<RequestQueue> Create(
      scoped_refptr<StatelessDevice> device);

  RequestQueue(scoped_refptr<StatelessDevice> device);
  ~RequestQueue();

  // Returns a request FD that was created by the driver.
  base::ScopedFD CreateRequestFD();

  // Pass only the data necessary for the driver to determine what output
  // formats the decoded bitstream can provide.
  bool SetHeadersForFormatNegotiation(void* ctrls);

  // Pass all of the data that the request api needs to start decoding a frame.
  bool QueueRequest(void* ctrls, const base::ScopedFD& request_fd);

 private:
  scoped_refptr<StatelessDevice> device_;
};

}  // namespace media
#endif  // MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
