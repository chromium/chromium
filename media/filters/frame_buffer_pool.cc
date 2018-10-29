// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_buffer_pool.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace media {

struct FrameBufferPool::FrameBuffer {
  // Not using std::vector<uint8_t> as resize() calls take a really long time
  // for large buffers.
  std::unique_ptr<uint8_t[]> data;
  size_t data_size = 0u;
  std::unique_ptr<uint8_t[]> alpha_data;
  size_t alpha_data_size = 0u;
  bool held_by_library = false;
  // Needs to be a counter since a frame buffer might be used multiple times.
  int held_by_frame = 0;
  base::TimeTicks last_use_time;
};

FrameBufferPool::FrameBufferPool()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FrameBufferPool::~FrameBufferPool() {
  DCHECK(in_shutdown_);

  // May be destructed on any thread.
}

uint8_t* FrameBufferPool::GetFrameBuffer(size_t min_size, void** fb_priv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!in_shutdown_);

  if (!registered_dump_provider_) {
    base::trace_event::MemoryDumpManager::GetInstance()
        ->RegisterDumpProviderWithSequencedTaskRunner(
            this, "FrameBufferPool", base::SequencedTaskRunnerHandle::Get(),
            MemoryDumpProvider::Options());
    registered_dump_provider_ = true;
  }

  // Check if a free frame buffer exists.
  auto it = std::find_if(
      frame_buffers_.begin(), frame_buffers_.end(),
      [](const std::unique_ptr<FrameBuffer>& fb) { return !IsUsed(fb.get()); });

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
    frame_buffer->data.reset(new uint8_t[min_size]);
    frame_buffer->data_size = min_size;
  }

  // Provide the client with a private identifier.
  *fb_priv = frame_buffer.get();
  return frame_buffer->data.get();
}

void FrameBufferPool::ReleaseFrameBuffer(void* fb_priv) {
  DCHECK(fb_priv);

  // Note: The library may invoke this method multiple times for the same frame,
  // so we can't DCHECK that |held_by_library| is true.
  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  frame_buffer->held_by_library = false;

  if (!IsUsed(frame_buffer))
    frame_buffer->last_use_time = tick_clock_->NowTicks();
}

uint8_t* FrameBufferPool::AllocateAlphaPlaneForFrameBuffer(size_t min_size,
                                                           void* fb_priv) {
  DCHECK(fb_priv);

  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  DCHECK(IsUsed(frame_buffer));
  if (frame_buffer->alpha_data_size < min_size) {
    // Free the existing |alpha_data| first so that the memory can be reused,
    // if possible. Note that the new array is purposely not initialized.
    frame_buffer->alpha_data.reset();
    frame_buffer->alpha_data.reset(new uint8_t[min_size]);
    frame_buffer->alpha_data_size = min_size;
  }
  return frame_buffer->alpha_data.get();
}

base::Closure FrameBufferPool::CreateFrameCallback(void* fb_priv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* frame_buffer = static_cast<FrameBuffer*>(fb_priv);
  ++frame_buffer->held_by_frame;

  return base::Bind(&FrameBufferPool::OnVideoFrameDestroyed, this,
                    base::SequencedTaskRunnerHandle::Get(), frame_buffer);
}

bool FrameBufferPool::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::trace_event::MemoryAllocatorDump* memory_dump =
      pmd->CreateAllocatorDump("media/frame_buffers/memory_pool");
  base::trace_event::MemoryAllocatorDump* used_memory_dump =
      pmd->CreateAllocatorDump("media/frame_buffers/memory_pool/used");

  pmd->AddSuballocation(memory_dump->guid(),
                        base::trace_event::MemoryDumpManager::GetInstance()
                            ->system_allocator_pool_name());
  size_t bytes_used = 0;
  size_t bytes_reserved = 0;
  for (const auto& frame_buffer : frame_buffers_) {
    if (IsUsed(frame_buffer.get()))
      bytes_used += frame_buffer->data_size + frame_buffer->alpha_data_size;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  in_shutdown_ = true;

  if (registered_dump_provider_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }

  // Clear any refs held by the library which isn't good about cleaning up after
  // itself. This is safe since the library has already been shutdown by this
  // point.
  for (const auto& frame_buffer : frame_buffers_)
    frame_buffer->held_by_library = false;

  EraseUnusedResources();
}

// static
bool FrameBufferPool::IsUsed(const FrameBuffer* buf) {
  return buf->held_by_library || buf->held_by_frame > 0;
}

void FrameBufferPool::EraseUnusedResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::EraseIf(frame_buffers_, [](const std::unique_ptr<FrameBuffer>& buf) {
    return !IsUsed(buf.get());
  });
}

void FrameBufferPool::OnVideoFrameDestroyed(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    FrameBuffer* frame_buffer) {
  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&FrameBufferPool::OnVideoFrameDestroyed, this,
                                  task_runner, frame_buffer));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(frame_buffer->held_by_frame, 0);
  --frame_buffer->held_by_frame;

  if (in_shutdown_) {
    // If we're in shutdown we can be sure that the library has been destroyed.
    EraseUnusedResources();
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  if (!IsUsed(frame_buffer))
    frame_buffer->last_use_time = now;

  base::EraseIf(frame_buffers_, [now](const std::unique_ptr<FrameBuffer>& buf) {
    return !IsUsed(buf.get()) &&
           now - buf->last_use_time >
               base::TimeDelta::FromSeconds(kStaleFrameLimitSecs);
  });
}

}  // namespace media
