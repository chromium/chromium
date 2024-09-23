// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_PASSTHROUGH_SYSMEM_BUFFER_STREAM_H_
#define MEDIA_FUCHSIA_COMMON_PASSTHROUGH_SYSMEM_BUFFER_STREAM_H_

#include "media/fuchsia/common/sysmem_buffer_stream.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/fuchsia/common/vmo_buffer_writer_queue.h"

namespace media {

// A SysmemBufferStream that simply writes the stream to sysmem buffers.
class MEDIA_EXPORT PassthroughSysmemBufferStream : public SysmemBufferStream {
 public:
  explicit PassthroughSysmemBufferStream(
      SysmemAllocatorClient* sysmem_allocator);
  ~PassthroughSysmemBufferStream() override;

  PassthroughSysmemBufferStream(const PassthroughSysmemBufferStream&) = delete;
  PassthroughSysmemBufferStream& operator=(
      const PassthroughSysmemBufferStream&) = delete;

  // SysmemBufferStream implementation:
  void Initialize(Sink* sink,
                  size_t min_buffer_size,
                  size_t min_buffer_count) override;
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) override;
  void Reset() override;

 private:
  void OnBuffersAcquired(
      std::vector<VmoBuffer> buffers,
      const fuchsia::sysmem2::SingleBufferSettings& buffer_settings);

  // Callbacks for VmoBufferWriterQueue.
  void ProcessOutputPacket(const DecoderBuffer* buffer,
                           StreamProcessorHelper::IoPacket packet);
  void ProcessEndOfStream();

  Sink* sink_ = nullptr;

  SysmemAllocatorClient* const sysmem_allocator_;
  std::unique_ptr<SysmemCollectionClient> output_buffer_collection_;

  VmoBufferWriterQueue queue_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_PASSTHROUGH_SYSMEM_BUFFER_STREAM_H_
