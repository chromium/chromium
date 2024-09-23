// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/passthrough_sysmem_buffer_stream.h"

#include "media/base/decoder_buffer.h"

namespace media {

PassthroughSysmemBufferStream::PassthroughSysmemBufferStream(
    SysmemAllocatorClient* sysmem_allocator)
    : sysmem_allocator_(sysmem_allocator) {
  DCHECK(sysmem_allocator_);
}

PassthroughSysmemBufferStream::~PassthroughSysmemBufferStream() = default;

void PassthroughSysmemBufferStream::Initialize(Sink* sink,
                                               size_t min_buffer_size,
                                               size_t min_buffer_count) {
  DCHECK(sink);
  sink_ = sink;

  fuchsia::sysmem2::BufferCollectionConstraints buffer_constraints =
      VmoBuffer::GetRecommendedConstraints(min_buffer_count, min_buffer_size,
                                           /*writable=*/true);

  // Create buffer collection.
  output_buffer_collection_ = sysmem_allocator_->AllocateNewCollection();
  output_buffer_collection_->CreateSharedToken(
      base::BindOnce(&Sink::OnSysmemBufferStreamBufferCollectionToken,
                     base::Unretained(sink_)));
  output_buffer_collection_->Initialize(std::move(buffer_constraints),
                                        "CrPassthroughSysmemBufferStream");
  output_buffer_collection_->AcquireBuffers(
      base::BindOnce(&PassthroughSysmemBufferStream::OnBuffersAcquired,
                     base::Unretained(this)));
}

void PassthroughSysmemBufferStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  queue_.EnqueueBuffer(std::move(buffer));
}

void PassthroughSysmemBufferStream::Reset() {
  queue_.ResetQueue();
}

void PassthroughSysmemBufferStream::OnBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  queue_.Start(
      std::move(buffers),
      base::BindRepeating(&PassthroughSysmemBufferStream::ProcessOutputPacket,
                          base::Unretained(this)),
      base::BindRepeating(&PassthroughSysmemBufferStream::ProcessEndOfStream,
                          base::Unretained(this)));
}

void PassthroughSysmemBufferStream::ProcessOutputPacket(
    const DecoderBuffer* buffer,
    StreamProcessorHelper::IoPacket packet) {
  sink_->OnSysmemBufferStreamOutputPacket(std::move(packet));
}

void PassthroughSysmemBufferStream::ProcessEndOfStream() {
  sink_->OnSysmemBufferStreamEndOfStream();
}

}  // namespace media
