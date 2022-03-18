// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_allocator.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_type_conversion.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

// cdm::Buffer implementation that provides access to mojo shared memory.
// It owns the memory until Destroy() is called.
class MojoCdmBuffer final : public cdm::Buffer {
 public:
  static MojoCdmBuffer* Create(
      base::UnsafeSharedMemoryRegion region,
      MojoSharedBufferVideoFrame::MojoSharedBufferDoneCB
          mojo_shared_buffer_done_cb) {
    DCHECK(region.IsValid());
    DCHECK(mojo_shared_buffer_done_cb);

    // cdm::Buffer interface limits capacity to uint32.
    CHECK_LE(region.GetSize(), std::numeric_limits<uint32_t>::max());

    base::WritableSharedMemoryMapping mapping = region.Map();
    if (!mapping.IsValid())
      return nullptr;

    return new MojoCdmBuffer(std::move(region), std::move(mapping),
                             std::move(mojo_shared_buffer_done_cb));
  }

  MojoCdmBuffer(const MojoCdmBuffer&) = delete;
  MojoCdmBuffer& operator=(const MojoCdmBuffer&) = delete;

  // cdm::Buffer implementation.
  void Destroy() final {
    // If nobody has claimed the handle, then return it.
    if (region_.IsValid()) {
      std::move(mojo_shared_buffer_done_cb_).Run(std::move(region_));
    }

    // No need to exist anymore.
    delete this;
  }

  uint32_t Capacity() const final { return mapping_.size(); }

  uint8_t* Data() final { return mapping_.GetMemoryAs<uint8_t>(); }

  void SetSize(uint32_t size) final {
    DCHECK_LE(size, Capacity());
    size_ = size > Capacity() ? 0 : size;
  }

  uint32_t Size() const final { return size_; }

  const base::UnsafeSharedMemoryRegion& Region() const { return region_; }

  base::UnsafeSharedMemoryRegion TakeRegion() { return std::move(region_); }

 private:
  MojoCdmBuffer(base::UnsafeSharedMemoryRegion region,
                base::WritableSharedMemoryMapping mapping,
                MojoSharedBufferVideoFrame::MojoSharedBufferDoneCB
                    mojo_shared_buffer_done_cb)
      : region_(std::move(region)),
        mojo_shared_buffer_done_cb_(std::move(mojo_shared_buffer_done_cb)),
        mapping_(std::move(mapping)) {
    DCHECK(mapping_.IsValid());
  }

  ~MojoCdmBuffer() final {
    // Verify that the buffer has been returned so it can be reused.
    DCHECK(!region_.IsValid());
  }

  // Unsafe because of the requirements of VideoFrame; see
  // MojoSharedBufferVideoFrame for more details.
  base::UnsafeSharedMemoryRegion region_;
  MojoSharedBufferVideoFrame::MojoSharedBufferDoneCB
      mojo_shared_buffer_done_cb_;

  base::WritableSharedMemoryMapping mapping_;
  uint32_t size_ = 0;
};

// VideoFrameImpl that is able to create a MojoSharedBufferVideoFrame
// out of the data.
class MojoCdmVideoFrame final : public VideoFrameImpl {
 public:
  explicit MojoCdmVideoFrame(MojoSharedBufferVideoFrame::MojoSharedBufferDoneCB
                                 mojo_shared_buffer_done_cb)
      : mojo_shared_buffer_done_cb_(std::move(mojo_shared_buffer_done_cb)) {}

  MojoCdmVideoFrame(const MojoCdmVideoFrame&) = delete;
  MojoCdmVideoFrame& operator=(const MojoCdmVideoFrame&) = delete;

  ~MojoCdmVideoFrame() final = default;

  // VideoFrameImpl implementation.
  scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) final {
    DCHECK(FrameBuffer());

    MojoCdmBuffer* buffer = static_cast<MojoCdmBuffer*>(FrameBuffer());
    const gfx::Size frame_size(Size().width, Size().height);

    base::UnsafeSharedMemoryRegion region = buffer->TakeRegion();
    DCHECK(region.IsValid());

    // Clear FrameBuffer so that MojoCdmVideoFrame no longer has a reference
    // to it (memory will be transferred to MojoSharedBufferVideoFrame).
    SetFrameBuffer(nullptr);

    // Destroy the MojoCdmBuffer as it is no longer needed.
    buffer->Destroy();

    const uint32_t offsets[] = {PlaneOffset(cdm::kYPlane),
                                PlaneOffset(cdm::kUPlane),
                                PlaneOffset(cdm::kVPlane)};
    const int32_t strides[] = {static_cast<int32_t>(Stride(cdm::kYPlane)),
                               static_cast<int32_t>(Stride(cdm::kUPlane)),
                               static_cast<int32_t>(Stride(cdm::kVPlane))};
    scoped_refptr<MojoSharedBufferVideoFrame> frame =
        media::MojoSharedBufferVideoFrame::Create(
            ToMediaVideoFormat(Format()), frame_size, gfx::Rect(frame_size),
            natural_size, std::move(region), offsets, strides,
            base::Microseconds(Timestamp()));

    // |frame| could fail to be created if the memory can't be mapped into
    // this address space.
    if (frame) {
      frame->set_color_space(MediaColorSpace().ToGfxColorSpace());
      frame->SetMojoSharedBufferDoneCB(std::move(mojo_shared_buffer_done_cb_));
    }
    return frame;
  }

 private:
  MojoSharedBufferVideoFrame::MojoSharedBufferDoneCB
      mojo_shared_buffer_done_cb_;
};

}  // namespace

MojoCdmAllocator::MojoCdmAllocator() {}

MojoCdmAllocator::~MojoCdmAllocator() = default;

// Creates a cdm::Buffer, reusing an existing buffer if one is available.
// If not, a new buffer is created using AllocateNewRegion(). The caller is
// responsible for calling Destroy() on the buffer when it is no longer needed.
cdm::Buffer* MojoCdmAllocator::CreateCdmBuffer(size_t capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!capacity)
    return nullptr;

  // Reuse a shmem region in the free map if there is one that fits |capacity|.
  // Otherwise, create a new one.
  base::UnsafeSharedMemoryRegion region;
  auto found = available_regions_.lower_bound(capacity);
  if (found == available_regions_.end()) {
    region = AllocateNewRegion(capacity);
    if (!region.IsValid()) {
      return nullptr;
    }
  } else {
    region = std::move(found->second);
    available_regions_.erase(found);
  }

  // Ownership of `region` is passed to MojoCdmBuffer. When it is done with the
  // memory, it must call `AddRegionrToAvailableMap()` to make the memory
  // available for another MojoCdmBuffer.
  return MojoCdmBuffer::Create(
      std::move(region),
      base::BindOnce(&MojoCdmAllocator::AddRegionToAvailableMap,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Creates a new MojoCdmVideoFrame on every request.
std::unique_ptr<VideoFrameImpl> MojoCdmAllocator::CreateCdmVideoFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<MojoCdmVideoFrame>(
      base::BindOnce(&MojoCdmAllocator::AddRegionToAvailableMap,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::UnsafeSharedMemoryRegion MojoCdmAllocator::AllocateNewRegion(
    size_t capacity) {
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
  if (available_regions_.size() >= kFreeLimit)
    available_regions_.erase(available_regions_.begin());

  // Creation of shared memory may be expensive if it involves synchronous IPC
  // calls. That's why we try to avoid AllocateNewRegion() as much as we can.
  base::CheckedNumeric<size_t> requested_capacity(capacity);
  requested_capacity += kBufferPadding;
  auto region =
      base::UnsafeSharedMemoryRegion::Create(requested_capacity.ValueOrDie());
  return region;
}

void MojoCdmAllocator::AddRegionToAvailableMap(
    base::UnsafeSharedMemoryRegion region) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t capacity = region.GetSize();
  available_regions_.insert({capacity, std::move(region)});
}

const base::UnsafeSharedMemoryRegion& MojoCdmAllocator::GetRegionForTesting(
    cdm::Buffer* buffer) const {
  MojoCdmBuffer* mojo_buffer = static_cast<MojoCdmBuffer*>(buffer);
  return mojo_buffer->Region();
}

size_t MojoCdmAllocator::GetAvailableRegionCountForTesting() {
  return available_regions_.size();
}

}  // namespace media
