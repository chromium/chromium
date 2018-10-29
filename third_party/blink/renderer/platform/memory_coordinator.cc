// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/memory_coordinator.h"

#include "base/sys_info.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

namespace blink {

// Wrapper function defined in WebKit.h
void DecommitFreeableMemory() {
  WTF::Partitions::DecommitFreeableMemory();
}

// static
bool MemoryCoordinator::is_low_end_device_ = false;

// static
bool MemoryCoordinator::IsLowEndDevice() {
  return is_low_end_device_;
}

// static
bool MemoryCoordinator::IsCurrentlyLowMemory() {
#if defined(OS_ANDROID)
  return base::android::SysUtils::IsCurrentlyLowMemory();
#else
  return false;
#endif
}

// static
void MemoryCoordinator::Initialize() {
  is_low_end_device_ = ::base::SysInfo::IsLowEndDevice();
  ApproximatedDeviceMemory::Initialize();
}

// static
void MemoryCoordinator::SetIsLowEndDeviceForTesting(bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

// static
MemoryCoordinator& MemoryCoordinator::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistent<MemoryCoordinator>,
                                  external, (new MemoryCoordinator));
  return *external.Get();
}

void MemoryCoordinator::RegisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.insert(thread);
}

void MemoryCoordinator::UnregisterThread(Thread* thread) {
  MutexLocker lock(threads_mutex_);
  threads_.erase(thread);
}

MemoryCoordinator::MemoryCoordinator() = default;

void MemoryCoordinator::RegisterClient(MemoryCoordinatorClient* client) {
  DCHECK(IsMainThread());
  DCHECK(client);
  DCHECK(!clients_.Contains(client));
  clients_.insert(client);
}

void MemoryCoordinator::UnregisterClient(MemoryCoordinatorClient* client) {
  DCHECK(IsMainThread());
  clients_.erase(client);
}

void MemoryCoordinator::OnMemoryPressure(WebMemoryPressureLevel level) {
  TRACE_EVENT0("blink", "MemoryCoordinator::onMemoryPressure");
  for (auto& client : clients_)
    client->OnMemoryPressure(level);
  if (level == kWebMemoryPressureLevelCritical)
    ClearMemory();
  WTF::Partitions::DecommitFreeableMemory();
}

void MemoryCoordinator::OnMemoryStateChange(MemoryState state) {
  for (auto& client : clients_)
    client->OnMemoryStateChange(state);
}

void MemoryCoordinator::OnPurgeMemory() {
  for (auto& client : clients_)
    client->OnPurgeMemory();
  // Don't call clearMemory() because font cache invalidation always causes full
  // layout. This increases tab switching cost significantly (e.g.
  // en.wikipedia.org/wiki/Wikipedia). So we should not invalidate the font
  // cache in purge+throttle.
  ImageDecodingStore::Instance().Clear();
  WTF::Partitions::DecommitFreeableMemory();

  // Thread-specific data never issues a layout, so we are safe here.
  MutexLocker lock(threads_mutex_);
  for (auto* thread : threads_) {
    if (!thread->GetTaskRunner())
      continue;

    PostCrossThreadTask(
        *thread->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(MemoryCoordinator::ClearThreadSpecificMemory));
  }
}

void MemoryCoordinator::ClearMemory() {
  // Clear the image cache.
  // TODO(tasak|bashi): Make ImageDecodingStore and FontCache be
  // MemoryCoordinatorClients rather than clearing caches here.
  ImageDecodingStore::Instance().Clear();
  FontGlobalContext::ClearMemory();
}

void MemoryCoordinator::ClearThreadSpecificMemory() {
  FontGlobalContext::ClearMemory();
}

void MemoryCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
}

}  // namespace blink
