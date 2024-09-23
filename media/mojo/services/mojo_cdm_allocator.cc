// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/services/mojo_cdm_allocator.h"

#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "media/base/video_frame.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/cdm_type_conversion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

using RecycleRegionCB =
    base::OnceCallback<void(std::unique_ptr<base::MappedReadOnlyRegion>)>;

// cdm::Buffer implementation that provides access to mojo shared memory.
// It owns the memory until Destroy() is called.
class MojoCdmBuffer final : public cdm::Buffer {
 public:
  MojoCdmBuffer(std::unique_ptr<base::MappedReadOnlyRegion> mapped_region,
                RecycleRegionCB recycle_region_cb)
      : mapped_region_(std::move(mapped_region)),
        recycle_region_cb_(std::move(recycle_region_cb)) {
    DCHECK(mapped_region_);
    DCHECK(mapped_region_->IsValid());
    DCHECK(recycle_region_cb_);
    // cdm::Buffer interface limits capacity to uint32.
    CHECK_LE(mapped_region_->region.GetSize(),
             std::numeric_limits<uint32_t>::max());
  }

  MojoCdmBuffer(const MojoCdmBuffer&) = delete;
  MojoCdmBuffer& operator=(const MojoCdmBuffer&) = delete;

  // cdm::Buffer implementation.
  void Destroy() final {
    // If nobody has claimed the handle, then return it.
    if (mapped_region_ && mapped_region_->IsValid()) {
      std::move(recycle_region_cb_).Run(std::move(mapped_region_));
    }

    // No need to exist anymore.
    delete this;
  }

  uint32_t Capacity() const final { return mapped_region_->mapping.size(); }

  uint8_t* Data() final {
    return mapped_region_->mapping.GetMemoryAs<uint8_t>();
  }

  void SetSize(uint32_t size) final {
    DCHECK_LE(size, Capacity());
    size_ = size > Capacity() ? 0 : size;
  }

  uint32_t Size() const final { return size_; }

  const base::MappedReadOnlyRegion& Region() const { return *mapped_region_; }
  std::unique_ptr<base::MappedReadOnlyRegion> TakeRegion() {
    return std::move(mapped_region_);
  }

 private:
  ~MojoCdmBuffer() final {
    // Verify that the buffer has been returned so it can be reused.
    DCHECK(!mapped_region_);
  }

  std::unique_ptr<base::MappedReadOnlyRegion> mapped_region_;
  RecycleRegionCB recycle_region_cb_;
  uint32_t size_ = 0;
};

// VideoFrameImpl that is able to create a STORAGE_SHMEM VideoFrame
// out of the data.
class MojoCdmVideoFrame final : public VideoFrameImpl {
 public:
  explicit MojoCdmVideoFrame(RecycleRegionCB recycle_region_cb)
      : recycle_region_cb_(std::move(recycle_region_cb)) {}

  MojoCdmVideoFrame(const MojoCdmVideoFrame&) = delete;
  MojoCdmVideoFrame& operator=(const MojoCdmVideoFrame&) = delete;

  ~MojoCdmVideoFrame() final = default;

  // VideoFrameImpl implementation.
  scoped_refptr<media::VideoFrame> TransformToVideoFrame(
      gfx::Size natural_size) final {
    DCHECK(FrameBuffer());

    MojoCdmBuffer* buffer = static_cast<MojoCdmBuffer*>(FrameBuffer());
    const gfx::Size frame_size(Size().width, Size().height);

    std::unique_ptr<base::MappedReadOnlyRegion> mapped_region =
        buffer->TakeRegion();
    DCHECK(mapped_region);
    DCHECK(mapped_region->region.IsValid());

    // Clear FrameBuffer so that MojoCdmVideoFrame no longer has a reference
    // to it (memory will be transferred to media::VideoFrame).
    SetFrameBuffer(nullptr);

    // Destroy the MojoCdmBuffer as it is no longer needed.
    buffer->Destroy();

    uint8_t* data =
        const_cast<uint8_t*>(mapped_region->mapping.GetMemoryAs<uint8_t>());
    if (PlaneOffset(cdm::kYPlane) != 0u) {
      LOG(ERROR) << "The first buffer offset is not 0";
      return nullptr;
    }
    auto frame = media::VideoFrame::WrapExternalYuvData(
        ToMediaVideoFormat(Format()), frame_size, gfx::Rect(frame_size),
        natural_size, static_cast<int32_t>(Stride(cdm::kYPlane)),
        static_cast<int32_t>(Stride(cdm::kUPlane)),
        static_cast<int32_t>(Stride(cdm::kVPlane)),
        data + PlaneOffset(cdm::kYPlane), data + PlaneOffset(cdm::kUPlane),
        data + PlaneOffset(cdm::kVPlane), base::Microseconds(Timestamp()));

    // |frame| could fail to be created if the memory can't be mapped into
    // this address space.
    // TODO(b/183748013): Set HDRMetadata once supported by the CDM interface.
    if (frame) {
      frame->metadata().power_efficient = false;
      frame->set_color_space(MediaColorSpace().ToGfxColorSpace());
      frame->BackWithSharedMemory(&mapped_region->region);
      frame->AddDestructionObserver(base::BindOnce(
          std::move(recycle_region_cb_), std::move(mapped_region)));
    }
    return frame;
  }

 private:
  RecycleRegionCB recycle_region_cb_;
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
  std::unique_ptr<base::MappedReadOnlyRegion> mapped_region;
  auto found = available_regions_.lower_bound(capacity);
  if (found == available_regions_.end()) {
    mapped_region = AllocateNewRegion(capacity);
    if (!mapped_region->IsValid()) {
      return nullptr;
    }
  } else {
    mapped_region = std::move(found->second);
    available_regions_.erase(found);
  }

  // Ownership of `region` is passed to MojoCdmBuffer. When it is done with the
  // memory, it must call `AddRegionrToAvailableMap()` to make the memory
  // available for another MojoCdmBuffer.
  return new MojoCdmBuffer(
      std::move(mapped_region),
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

std::unique_ptr<base::MappedReadOnlyRegion> MojoCdmAllocator::AllocateNewRegion(
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
  return std::make_unique<base::MappedReadOnlyRegion>(
      base::ReadOnlySharedMemoryRegion::Create(
          requested_capacity.ValueOrDie()));
}

void MojoCdmAllocator::AddRegionToAvailableMap(
    std::unique_ptr<base::MappedReadOnlyRegion> mapped_region) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(mapped_region);
  size_t capacity = mapped_region->region.GetSize();
  available_regions_.insert({capacity, std::move(mapped_region)});
}

const base::MappedReadOnlyRegion& MojoCdmAllocator::GetRegionForTesting(
    cdm::Buffer* buffer) const {
  MojoCdmBuffer* mojo_buffer = static_cast<MojoCdmBuffer*>(buffer);
  return mojo_buffer->Region();
}

size_t MojoCdmAllocator::GetAvailableRegionCountForTesting() {
  return available_regions_.size();
}

}  // namespace media
