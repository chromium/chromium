// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_ahb.h"

#include <android/hardware_buffer.h>

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

MappableBufferAHB::~MappableBufferAHB() {
  base::AutoLock auto_lock(map_lock_);
  CHECK(!async_mapping_in_progress_);
  CHECK_EQ(map_count_, 0u);
}

// static
std::unique_ptr<MappableBufferAHB> MappableBufferAHB::CreateFromHandle(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    viz::SharedImageFormat format,
    CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool) {
  DCHECK_EQ(handle.type, gfx::ANDROID_HARDWARE_BUFFER);
  return base::WrapUnique(new MappableBufferAHB(
      size, format, std::move(handle),
      std::move(copy_native_buffer_to_shmem_callback), std::move(pool)));
}

// static
base::OnceClosure MappableBufferAHB::AllocateForTesting(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  DCHECK(format == viz::MultiPlaneFormat::kNV12);

  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc desc = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .layers = 1,
      .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
      .usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
  };

  AHardwareBuffer_allocate(&desc, &buffer);
  *handle = gfx::GpuMemoryBufferHandle(
      base::android::ScopedHardwareBufferHandle::Adopt(buffer));
  return base::DoNothing();
}

bool MappableBufferAHB::Map() {
  base::WaitableEvent event;
  bool result = false;
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  MapAsync(base::BindOnce(
      [](base::WaitableEvent* event, bool* result_ptr, bool map_result) {
        *result_ptr = map_result;
        event->Signal();
      },
      &event, &result));
  event.Wait();
  return result;
}

void MappableBufferAHB::MapAsync(base::OnceCallback<void(bool)> result_cb) {
  std::optional<bool> early_result;
  absl::Cleanup run_callback_if_needed = [&] {
    if (result_cb) {
      CHECK(early_result.has_value());
      std::move(result_cb).Run(early_result.value());
    }
  };

  base::AutoLock auto_lock(map_lock_);
  if (map_count_ > 0) {
    ++map_count_;
    early_result = true;
    return;
  }

  CHECK(copy_native_buffer_to_shmem_callback_);
  CHECK(shared_memory_pool_);

  size_t buffer_size =
      viz::SharedMemorySizeForSharedImageFormat(format_, size_).value();
  if (!shared_memory_handle_) {
    shared_memory_handle_ =
        shared_memory_pool_->MaybeAllocateBuffer(buffer_size);
    if (!shared_memory_handle_) {
      early_result = false;
      return;
    }
  }

  map_callbacks_.push_back(std::move(result_cb));
  if (async_mapping_in_progress_) {
    return;
  }
  async_mapping_in_progress_ = true;
  // Need to perform mapping in GPU process
  // Unretained is safe because of GMB isn't destroyed before the callback
  // executes. This is CHECKed in the destructor.
  copy_native_buffer_to_shmem_callback_.Run(
      CloneHandle(), shared_memory_handle_->GetRegion().Duplicate(),
      base::BindOnce(&MappableBufferAHB::CheckAsyncMapResult,
                     base::Unretained(this)));
}

void MappableBufferAHB::CheckAsyncMapResult(bool result) {
  std::vector<base::OnceCallback<void(bool)>> map_callbacks;
  {
    // Must not hold the lock during the callbacks calls.
    base::AutoLock auto_lock(map_lock_);
    CHECK_EQ(map_count_, 0u);
    CHECK(async_mapping_in_progress_);

    if (result) {
      map_count_ += map_callbacks_.size();
    }

    async_mapping_in_progress_ = false;
    swap(map_callbacks_, map_callbacks);
  }
  for (auto& cb : map_callbacks) {
    std::move(cb).Run(result);
  }
}

bool MappableBufferAHB::AsyncMappingIsNonBlocking() const {
  return true;
}

void* MappableBufferAHB::memory(size_t plane) {
  AssertMapped();

  if (static_cast<int>(plane) > format_.NumberOfPlanes() ||
      !shared_memory_handle_) {
    return nullptr;
  }

  base::span<uint8_t> mapping =
      shared_memory_handle_->GetMapping().GetMemoryAsSpan<uint8_t>();
  size_t offset =
      viz::SharedMemoryOffsetForSharedImageFormat(format_, plane, size_);
  return mapping.subspan(offset).data();
}

void MappableBufferAHB::Unmap() {
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
  CHECK(!async_mapping_in_progress_);
  if (--map_count_) {
    return;
  }

  if (shared_memory_handle_) {
    shared_memory_handle_.reset();
  }
}

int MappableBufferAHB::stride(size_t plane) const {
  size_t stride = viz::SharedMemoryRowSizeForSharedImageFormat(format_, plane,
                                                               size_.width())
                      .value();
  return static_cast<int>(stride);
}

gfx::GpuMemoryBufferType MappableBufferAHB::GetType() const {
  return gfx::ANDROID_HARDWARE_BUFFER;
}

gfx::GpuMemoryBufferHandle MappableBufferAHB::CloneHandle() const {
  return handle_.Clone();
}

void MappableBufferAHB::AssertMapped() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(map_lock_);
  DCHECK_GT(map_count_, 0u);
#endif
}

MappableBufferAHB::MappableBufferAHB(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::GpuMemoryBufferHandle handle,
    CopyNativeBufferToShMemCallback copy_native_buffer_to_shmem_callback,
    scoped_refptr<base::UnsafeSharedMemoryPool> pool)
    : size_(size),
      format_(format),
      handle_(std::move(handle)),
      copy_native_buffer_to_shmem_callback_(
          std::move(copy_native_buffer_to_shmem_callback)),
      shared_memory_pool_(std::move(pool)) {}

}  // namespace gpu
