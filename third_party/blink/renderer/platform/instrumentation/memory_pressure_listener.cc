// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

namespace blink {

// Wrapper function defined in WebKit.h
void DecommitFreeableMemory() {
  CHECK(IsMainThread());
  base::PartitionAllocMemoryReclaimer::Instance()->Reclaim();
}

// static
bool MemoryPressureListenerRegistry::is_low_end_device_ = false;

// static
bool MemoryPressureListenerRegistry::IsLowEndDevice() {
  return is_low_end_device_;
}

// static
bool MemoryPressureListenerRegistry::IsCurrentlyLowMemory() {
#if defined(OS_ANDROID)
  return base::android::SysUtils::IsCurrentlyLowMemory();
#else
  return false;
#endif
}

// static
void MemoryPressureListenerRegistry::Initialize() {
  is_low_end_device_ = ::base::SysInfo::IsLowEndDevice();
  ApproximatedDeviceMemory::Initialize();
  // Make sure the instance of MemoryPressureListenerRegistry is created on
  // the main thread. Otherwise we might try to create the instance on a
  // thread which doesn't have ThreadState (e.g., the IO thread).
  MemoryPressureListenerRegistry::Instance();
}

// static
void MemoryPressureListenerRegistry::SetIsLowEndDeviceForTesting(
    bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

// static
MemoryPressureListenerRegistry& MemoryPressureListenerRegistry::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CrossThreadPersistent<MemoryPressureListenerRegistry>, external,
      (MakeGarbageCollected<MemoryPressureListenerRegistry>()));
  return *external.Get();
}

void MemoryPressureListenerRegistry::RegisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.insert(thread);
}

void MemoryPressureListenerRegistry::UnregisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.erase(thread);
}

MemoryPressureListenerRegistry::MemoryPressureListenerRegistry() = default;

void MemoryPressureListenerRegistry::RegisterClient(
    MemoryPressureListener* client) {
  DCHECK(IsMainThread());
  DCHECK(client);
  DCHECK(!clients_.Contains(client));
  clients_.insert(client);
}

void MemoryPressureListenerRegistry::UnregisterClient(
    MemoryPressureListener* client) {
  DCHECK(IsMainThread());
  clients_.erase(client);
}

void MemoryPressureListenerRegistry::OnMemoryPressure(
    WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryPressureListenerRegistry::onMemoryPressure");
  CHECK(IsMainThread());
  for (auto& client : clients_)
    client->OnMemoryPressure(level);
  base::PartitionAllocMemoryReclaimer::Instance()->Reclaim();
}

void MemoryPressureListenerRegistry::OnPurgeMemory() {
  CHECK(IsMainThread());
  for (auto& client : clients_)
    client->OnPurgeMemory();
  ImageDecodingStore::Instance().Clear();
  base::PartitionAllocMemoryReclaimer::Instance()->Reclaim();

  // Thread-specific data never issues a layout, so we are safe here.
  MutexLocker lock(threads_mutex_);
  for (auto* thread : threads_) {
    if (!thread->GetTaskRunner())
      continue;

    PostCrossThreadTask(
        *thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            MemoryPressureListenerRegistry::ClearThreadSpecificMemory));
  }
}

void MemoryPressureListenerRegistry::ClearThreadSpecificMemory() {
  FontGlobalContext::ClearMemory();
}

void MemoryPressureListenerRegistry::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
}

}  // namespace blink
