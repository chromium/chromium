// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_allocator.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_type_conversion.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

typedef base::Callback<void(mojo::ScopedSharedBufferHandle buffer,
                            size_t capacity)>
    MojoSharedBufferDoneCB;

// cdm::Buffer implementation that provides access to mojo shared memory.
// It owns the memory until Destroy() is called.
class MojoCdmBuffer : public cdm::Buffer {
 public:
  static MojoCdmBuffer* Create(
      mojo::ScopedSharedBufferHandle buffer,
      size_t capacity,
      const MojoSharedBufferDoneCB& mojo_shared_buffer_done_cb) {
    DCHECK(buffer.is_valid());
    DCHECK(mojo_shared_buffer_done_cb);

    // cdm::Buffer interface limits capacity to uint32.
    DCHECK_LE(capacity, std::numeric_limits<uint32_t>::max());
    return new MojoCdmBuffer(std::move(buffer),
                             base::checked_cast<uint32_t>(capacity),
                             mojo_shared_buffer_done_cb);
  }

  // cdm::Buffer implementation.
  void Destroy() final {
    // Unmap the memory before returning the handle to |allocator_|.
    mapping_.reset();

    // If nobody has claimed the handle, then return it.
    if (buffer_.is_valid())
      mojo_shared_buffer_done_cb_.Run(std::move(buffer_), capacity_);

    // No need to exist anymore.
    delete this;
  }

  uint32_t Capacity() const final { return capacity_; }

  uint8_t* Data() final { return static_cast<uint8_t*>(mapping_.get()); }

  void SetSize(uint32_t size) final {
    DCHECK_LE(size, Capacity());
    size_ = size > Capacity() ? 0 : size;
  }

  uint32_t Size() const final { return size_; }

  const mojo::SharedBufferHandle& Handle() const { return buffer_.get(); }

  mojo::ScopedSharedBufferHandle TakeHandle() { return std::move(buffer_); }

 private:
  MojoCdmBuffer(mojo::ScopedSharedBufferHandle buffer,
                uint32_t capacity,
                const MojoSharedBufferDoneCB& mojo_shared_buffer_done_cb)
      : buffer_(std::move(buffer)),
        mojo_shared_buffer_done_cb_(mojo_shared_buffer_done_cb),
        capacity_(capacity),
        size_(0) {
    mapping_ = buffer_->Map(capacity_);
    DCHECK(mapping_);
  }

  ~MojoCdmBuffer() final {
    // Verify that the buffer has been returned so it can be reused.
    DCHECK(!buffer_.is_valid());
  }

  mojo::ScopedSharedBufferHandle buffer_;
  MojoSharedBufferDoneCB mojo_shared_buffer_done_cb_;

  mojo::ScopedSharedBufferMapping mapping_;
  uint32_t capacity_;
  uint32_t size_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmBuffer);
};

// VideoFrameImpl that is able to create a MojoSharedBufferVideoFrame
// out of the data.
class MojoCdmVideoFrame : public VideoFrameImpl {
 public:
  explicit MojoCdmVideoFrame(
      const MojoSharedBufferDoneCB& mojo_shared_buffer_done_cb)
      : mojo_shared_buffer_done_cb_(mojo_shared_buffer_done_cb) {}
  ~MojoCdmVideoFrame() final = default;

  // VideoFrameImpl implementation.
  scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) final {
    DCHECK(FrameBuffer());

    MojoCdmBuffer* buffer = static_cast<MojoCdmBuffer*>(FrameBuffer());
    const gfx::Size frame_size(Size().width, Size().height);

    // Take ownership of the mojo::ScopedSharedBufferHandle from |buffer|.
    uint32_t buffer_size = buffer->Size();
    mojo::ScopedSharedBufferHandle handle = buffer->TakeHandle();
    DCHECK(handle.is_valid());

    // Clear FrameBuffer so that MojoCdmVideoFrame no longer has a reference
    // to it (memory will be transferred to MojoSharedBufferVideoFrame).
    SetFrameBuffer(nullptr);

    // Destroy the MojoCdmBuffer as it is no longer needed.
    buffer->Destroy();

    scoped_refptr<MojoSharedBufferVideoFrame> frame =
        media::MojoSharedBufferVideoFrame::Create(
            ToMediaVideoFormat(Format()), frame_size, gfx::Rect(frame_size),
            natural_size, std::move(handle), buffer_size,
            PlaneOffset(cdm::kYPlane), PlaneOffset(cdm::kUPlane),
            PlaneOffset(cdm::kVPlane), Stride(cdm::kYPlane),
            Stride(cdm::kUPlane), Stride(cdm::kVPlane),
            base::TimeDelta::FromMicroseconds(Timestamp()));

    frame->set_color_space(MediaColorSpace().ToGfxColorSpace());

    // |frame| could fail to be created if the memory can't be mapped into
    // this address space.
    if (frame)
      frame->SetMojoSharedBufferDoneCB(mojo_shared_buffer_done_cb_);
    return frame;
  }

 private:
  MojoSharedBufferDoneCB mojo_shared_buffer_done_cb_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmVideoFrame);
};

}  // namespace

MojoCdmAllocator::MojoCdmAllocator() {}

MojoCdmAllocator::~MojoCdmAllocator() = default;

// Creates a cdm::Buffer, reusing an existing buffer if one is available.
// If not, a new buffer is created using AllocateNewBuffer(). The caller is
// responsible for calling Destroy() on the buffer when it is no longer needed.
cdm::Buffer* MojoCdmAllocator::CreateCdmBuffer(size_t capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!capacity)
    return nullptr;

  // Reuse a buffer in the free map if there is one that fits |capacity|.
  // Otherwise, create a new one.
  mojo::ScopedSharedBufferHandle buffer;
  auto found = available_buffers_.lower_bound(capacity);
  if (found == available_buffers_.end()) {
    buffer = AllocateNewBuffer(&capacity);
    if (!buffer.is_valid())
      return nullptr;
  } else {
    capacity = found->first;
    buffer = std::move(found->second);
    available_buffers_.erase(found);
  }

  // Ownership of the SharedBufferHandle is passed to MojoCdmBuffer. When it is
  // done with the memory, it must call AddBufferToAvailableMap() to make the
  // memory available for another MojoCdmBuffer.
  return MojoCdmBuffer::Create(
      std::move(buffer), capacity,
      base::Bind(&MojoCdmAllocator::AddBufferToAvailableMap,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Creates a new MojoCdmVideoFrame on every request.
std::unique_ptr<VideoFrameImpl> MojoCdmAllocator::CreateCdmVideoFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<MojoCdmVideoFrame>(
      base::Bind(&MojoCdmAllocator::AddBufferToAvailableMap,
                 weak_ptr_factory_.GetWeakPtr()));
}

mojo::ScopedSharedBufferHandle MojoCdmAllocator::AllocateNewBuffer(
    size_t* capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always pad new allocated buffer so that we don't need to reallocate
  // buffers frequently if requested sizes fluctuate slightly.
  static const size_t kBufferPadding = 512;

  // Maximum number of free buffers we can keep when allocating new buffers.
  static const size_t kFreeLimit = 3;

  // Destroy the smallest buffer before allocating a new bigger buffer if the
  // number of free buffers exceeds a limit. This mechanism helps avoid ending
  // up with too many small buffers, which could happen if the size to be
  // allocated keeps increasing.
  if (available_buffers_.size() >= kFreeLimit)
    available_buffers_.erase(available_buffers_.begin());

  // Creation of shared memory may be expensive if it involves synchronous IPC
  // calls. That's why we try to avoid AllocateNewBuffer() as much as we can.
  base::CheckedNumeric<size_t> requested_capacity(*capacity);
  requested_capacity += kBufferPadding;
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(requested_capacity.ValueOrDie());
  if (!handle.is_valid())
    return handle;
  *capacity = requested_capacity.ValueOrDie();
  return handle;
}

void MojoCdmAllocator::AddBufferToAvailableMap(
    mojo::ScopedSharedBufferHandle buffer,
    size_t capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  available_buffers_.insert(std::make_pair(capacity, std::move(buffer)));
}

MojoHandle MojoCdmAllocator::GetHandleForTesting(cdm::Buffer* buffer) {
  MojoCdmBuffer* mojo_buffer = static_cast<MojoCdmBuffer*>(buffer);
  return mojo_buffer->Handle().value();
}

size_t MojoCdmAllocator::GetAvailableBufferCountForTesting() {
  return available_buffers_.size();
}

}  // namespace media
