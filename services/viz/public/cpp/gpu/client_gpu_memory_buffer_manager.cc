// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/client_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"

namespace viz {

ClientGpuMemoryBufferManager::ClientGpuMemoryBufferManager(
    mojo::PendingRemote<gpu::mojom::ClientGmbInterface> gpu_direct)
    : thread_("GpuMemoryThread"),
      gpu_memory_buffer_support_(
          std::make_unique<gpu::GpuMemoryBufferSupport>()),
      pool_(base::MakeRefCounted<base::UnsafeSharedMemoryPool>()) {
  CHECK(thread_.Start());
  // The thread is owned by this object. Which means the task will not run if
  // the object has been destroyed. So Unretained() is safe.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClientGpuMemoryBufferManager::InitThread,
                                base::Unretained(this), std::move(gpu_direct)));
}

ClientGpuMemoryBufferManager::~ClientGpuMemoryBufferManager() {
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ClientGpuMemoryBufferManager::TearDownThread,
                                base::Unretained(this)));
  thread_.Stop();
}

void ClientGpuMemoryBufferManager::InitThread(
    mojo::PendingRemote<gpu::mojom::ClientGmbInterface> gpu_direct_remote) {
    gpu_direct_.Bind(std::move(gpu_direct_remote));
    gpu_direct_.set_disconnect_handler(
        base::BindOnce(&ClientGpuMemoryBufferManager::DisconnectGpuOnThread,
                       base::Unretained(this)));
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

void ClientGpuMemoryBufferManager::TearDownThread() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  DisconnectGpuOnThread();
}

void ClientGpuMemoryBufferManager::DisconnectGpuOnThread() {
  gpu_direct_.reset();
  for (base::WaitableEvent* waiter : pending_allocation_waiters_) {
    waiter->Signal();
  }
  pending_allocation_waiters_.clear();
}

void ClientGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle,
    base::WaitableEvent* wait) {
  DCHECK(thread_.task_runner()->BelongsToCurrentThread());

  // |handle| and |wait| are both on the stack, and will be alive until |wait|
  // is signaled. So it is safe for OnGpuMemoryBufferAllocated() to operate on
  // these.
  if (gpu_direct_) {
    gpu_direct_->CreateGpuMemoryBuffer(
        gfx::GpuMemoryBufferId(++counter_), size, format, usage,
        gpu::kNullSurfaceHandle,
        base::BindOnce(
            &ClientGpuMemoryBufferManager::OnGpuMemoryBufferAllocatedOnThread,
            base::Unretained(this), handle, wait));
  } else {
    // If the interface are disconnected, we can't fulfill the request.
    wait->Signal();
    return;
  }
  pending_allocation_waiters_.insert(wait);
}

void ClientGpuMemoryBufferManager::OnGpuMemoryBufferAllocatedOnThread(
    gfx::GpuMemoryBufferHandle* ret_handle,
    base::WaitableEvent* wait,
    gfx::GpuMemoryBufferHandle handle) {
  auto it = pending_allocation_waiters_.find(wait);
  CHECK(it != pending_allocation_waiters_.end(), base::NotFatalUntil::M130);
  pending_allocation_waiters_.erase(it);

  *ret_handle = std::move(handle);
  wait->Signal();
}

void ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id) {
  if (!thread_.task_runner()->BelongsToCurrentThread()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                       base::Unretained(this), id));
    return;
  }
  if (gpu_direct_) {
    gpu_direct_->DestroyGpuMemoryBuffer(id);
  }
}

std::unique_ptr<gfx::GpuMemoryBuffer>
ClientGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  // Note: this can be called from multiple threads at the same time. Some of
  // those threads may not have a TaskRunner set.
  // One of such threads is a WebRTC encoder thread.
  // That thread is not owned by chromium and therefore doesn't have any
  // blocking scope machinery. But the workload there is supposed to happen
  // synchronously, because this is how the WebRTC architecture is designed.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
  DCHECK_EQ(gpu::kNullSurfaceHandle, surface_handle);
  CHECK(!thread_.task_runner()->BelongsToCurrentThread());
  gfx::GpuMemoryBufferHandle gmb_handle;
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientGpuMemoryBufferManager::AllocateGpuMemoryBufferOnThread,
          base::Unretained(this), size, format, usage, &gmb_handle, &wait));
  wait.Wait();
  if (gmb_handle.is_null())
    return nullptr;

  auto gmb_handle_id = gmb_handle.id;
  auto callback =
      base::BindOnce(&ClientGpuMemoryBufferManager::DeletedGpuMemoryBuffer,
                     weak_ptr_, gmb_handle_id);
  std::unique_ptr<gpu::GpuMemoryBufferImpl> buffer =
      gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
          std::move(gmb_handle), size, format, usage,
          base::BindPostTask(thread_.task_runner(), std::move(callback)), this,
          pool_);
  if (!buffer) {
    DeletedGpuMemoryBuffer(gmb_handle_id);
    return nullptr;
  }
  return std::move(buffer);
}

void ClientGpuMemoryBufferManager::CopyGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  if (!thread_.task_runner()->BelongsToCurrentThread()) {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ClientGpuMemoryBufferManager::CopyGpuMemoryBufferAsync,
                       base::Unretained(this), std::move(buffer_handle),
                       std::move(memory_region), std::move(callback)));
    return;
  }

  if (gpu_direct_) {
    gpu_direct_->CopyGpuMemoryBuffer(
        std::move(buffer_handle), std::move(memory_region),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                    /*result=*/false));
  }
}

bool ClientGpuMemoryBufferManager::CopyGpuMemoryBufferSync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  base::WaitableEvent event;
  bool mapping_result = false;
  // Note: this can be called from multiple threads at the same time. Some of
  // those threads may not have a TaskRunner set.
  // One of such threads is a WebRTC encoder thread.
  // That thread is not owned by chromium and therefore doesn't have any
  // blocking scope machinery. But the workload there is supposed to happen
  // synchronously, because this is how the WebRTC architecture is designed.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow;
  CopyGpuMemoryBufferAsync(
      std::move(buffer_handle), std::move(memory_region),
      base::BindOnce(
          [](base::WaitableEvent* event, bool* result_ptr, bool result) {
            *result_ptr = result;
            event->Signal();
          },
          &event, &mapping_result));
  event.Wait();
  return mapping_result;
}

}  // namespace viz
