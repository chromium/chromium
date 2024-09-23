// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/frame_buffer_pool.h"

#include "base/logging.h"

#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/process/memory.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace media {

// Helper class to allow thread safe memory dumping without a task runner.
class FrameBufferPool::FrameBufferMemoryDumpProviderImpl
    : public base::trace_event::MemoryDumpProvider {
 public:
  explicit FrameBufferMemoryDumpProviderImpl(
      scoped_refptr<FrameBufferPool> pool)
      : pool_(std::move(pool)) {
    DCHECK(pool_);
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "FrameBufferPool", nullptr);
  }

  ~FrameBufferMemoryDumpProviderImpl() override = default;

  // base::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override {
    return pool_->OnMemoryDump(args, pmd);
  }

 private:
  scoped_refptr<FrameBufferPool> pool_;
};

struct FrameBufferPool::FrameBuffer {
  // Not using std::vector<uint8_t> as resize() calls take a really long time
  // for large buffers.
  std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> data;
  size_t data_size = 0u;
  std::unique_ptr<uint8_t, base::UncheckedFreeDeleter> alpha_data;
  size_t alpha_data_size = 0u;
  bool held_by_library = false;
  // Needs to be a counter since a frame buffer might be used multiple times.
  int held_by_frame = 0;
  base::TimeTicks last_use_time;
};

FrameBufferPool::FrameBufferPool(bool zero_initialize_memory)
    : zero_initialize_memory_(zero_initialize_memory),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

FrameBufferPool::~FrameBufferPool() {
  base::AutoLock lock(lock_);
  DCHECK(in_shutdown_);

  // May be destructed on any thread.
}

uint8_t* FrameBufferPool::GetFrameBuffer(size_t min_size, void** fb_priv) {
  base::AutoLock lock(lock_);
  DCHECK(!in_shutdown_);

  if (!memory_dump_impl_) {
    memory_dump_impl_ =
        std::make_unique<FrameBufferMemoryDumpProviderImpl>(this);
  }

  // Check if a free frame buffer exists.
  auto it = base::ranges::find_if_not(frame_buffers_, &IsUsedLocked,
                                      &std::unique_ptr<FrameBuffer>::get);

  // If not, create one.
  if (it == frame_buffers_.end())
    it = frame_buffers_.insert(it, std::make_unique<FrameBuffer>());

  auto& frame_buffer = *it;

  // Resize the frame buffer if necessary.
  frame_buffer->held_by_library = true;
  if (frame_buffer->data_size < min_size) {
    // Free the existing |data| first so that the memory can be reused,
    // if possible. Note that the new array is purposely not initialized.
    frame_buffer->data.reset();

    uint8_t* data = nullptr;
    if (!force_allocation_error_) {
      bool result = false;
      if (zero_initialize_memory_) {
        result = base::UncheckedCalloc(1u, min_size,
                                       reinterpret_cast<void**>(&data));
      } else {
        result =
            base::UncheckedMalloc(min_size, reinterpret_cast<void**>(&data));
      }

      // Unclear why, but the docs indicate both that `data` will be null on
      // failure, and also that the return value must not be discarded.
      if (!result) {
        data = nullptr;
      }
    }

    if (!data) {
      frame_buffers_.erase(it);
      return nullptr;
    }

    frame_buffer->data.reset(data);
    frame_buffer->data_size = min_size;
  }

  // Provide the client with a private identifier.
  *fb_priv = frame_buffer.get();
  return frame_buffer->data.get();
}

void FrameBufferPool::ReleaseFrameBuffer(void* fb_priv) {
  base::AutoLock lock(lock_);
  DCHECK(fb_priv);

  // Note: The library may invoke this method multiple times for the same frame,
  // so we can't DCHECK that |held_by_library| is true.
  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  frame_buffer->held_by_library = false;

  if (!IsUsedLocked(frame_buffer)) {
    frame_buffer->last_use_time = tick_clock_->NowTicks();
  }
}

uint8_t* FrameBufferPool::AllocateAlphaPlaneForFrameBuffer(size_t min_size,
                                                           void* fb_priv) {
  base::AutoLock lock(lock_);
  DCHECK(fb_priv);

  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  DCHECK(IsUsedLocked(frame_buffer));
  if (frame_buffer->alpha_data_size < min_size) {
    // Free the existing |alpha_data| first so that the memory can be reused,
    // if possible. Note that the new array is purposely not initialized.
    frame_buffer->alpha_data.reset();
    uint8_t* data = nullptr;
    if (force_allocation_error_ ||
        !base::UncheckedMalloc(min_size, reinterpret_cast<void**>(&data)) ||
        !data) {
      return nullptr;
    }
    frame_buffer->alpha_data.reset(data);
    frame_buffer->alpha_data_size = min_size;
  }
  return frame_buffer->alpha_data.get();
}

base::OnceClosure FrameBufferPool::CreateFrameCallback(void* fb_priv) {
  base::AutoLock lock(lock_);

  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  ++frame_buffer->held_by_frame;

  return base::BindOnce(&FrameBufferPool::OnVideoFrameDestroyed, this,
                        frame_buffer);
}

bool FrameBufferPool::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  if (in_shutdown_) {
    return false;
  }

  base::trace_event::MemoryAllocatorDump* memory_dump =
      pmd->CreateAllocatorDump(
          base::StringPrintf("media/frame_buffers/memory_pool/0x%" PRIXPTR,
                             reinterpret_cast<uintptr_t>(this)));
  base::trace_event::MemoryAllocatorDump* used_memory_dump =
      pmd->CreateAllocatorDump(
          base::StringPrintf("media/frame_buffers/memory_pool/used/0x%" PRIXPTR,
                             reinterpret_cast<uintptr_t>(this)));

  auto* pool_name = base::trace_event::MemoryDumpManager::GetInstance()
                        ->system_allocator_pool_name();
  if (pool_name) {
    pmd->AddSuballocation(memory_dump->guid(), pool_name);
  }
  size_t bytes_used = 0;
  size_t bytes_reserved = 0;
  for (const auto& frame_buffer : frame_buffers_) {
    if (IsUsedLocked(frame_buffer.get())) {
      bytes_used += frame_buffer->data_size + frame_buffer->alpha_data_size;
    }
    bytes_reserved += frame_buffer->data_size + frame_buffer->alpha_data_size;
  }

  memory_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         bytes_reserved);
  used_memory_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes, bytes_used);

  return true;
}

void FrameBufferPool::Shutdown() {
  base::AutoLock lock(lock_);
  in_shutdown_ = true;

  // Hand over our dump implementation to the manager for eventual deletion.
  if (memory_dump_impl_) {
    base::trace_event::MemoryDumpManager::GetInstance()
        ->UnregisterAndDeleteDumpProviderSoon(std::move(memory_dump_impl_));
  }

  // Clear any refs held by the library which isn't good about cleaning up after
  // itself. This is safe since the library has already been shutdown by this
  // point.
  for (const auto& frame_buffer : frame_buffers_)
    frame_buffer->held_by_library = false;

  EraseUnusedResourcesLocked();
}

// static
bool FrameBufferPool::IsUsedLocked(const FrameBuffer* buf) {
  // Static, so can't check that `lock_` is acquired.
  return buf->held_by_library || buf->held_by_frame > 0;
}

void FrameBufferPool::EraseUnusedResourcesLocked() {
  lock_.AssertAcquired();
  std::erase_if(frame_buffers_, [](const std::unique_ptr<FrameBuffer>& buf) {
    return !IsUsedLocked(buf.get());
  });
}

void FrameBufferPool::OnVideoFrameDestroyed(FrameBuffer* frame_buffer) {
  base::AutoLock lock(lock_);
  DCHECK_GT(frame_buffer->held_by_frame, 0);
  --frame_buffer->held_by_frame;

  if (in_shutdown_) {
    // If we're in shutdown we can be sure that the library has been destroyed.
    EraseUnusedResourcesLocked();
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  if (!IsUsedLocked(frame_buffer)) {
    frame_buffer->last_use_time = now;
  }

  std::erase_if(frame_buffers_, [now](const std::unique_ptr<FrameBuffer>& buf) {
    return !IsUsedLocked(buf.get()) &&
           now - buf->last_use_time > base::Seconds(kStaleFrameLimitSecs);
  });
}

}  // namespace media
