// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
bool MemoryPressureListenerRegistry::is_low_end_device_ = false;

// static
bool MemoryPressureListenerRegistry::IsLowEndDevice() {
  return is_low_end_device_;
}

bool MemoryPressureListenerRegistry::
    IsLowEndDeviceOrPartialLowEndModeEnabled() {
  return is_low_end_device_ ||
         base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled();
}

bool MemoryPressureListenerRegistry::
    IsLowEndDeviceOrPartialLowEndModeEnabledIncludingCanvasFontCache() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return is_low_end_device_ ||
         base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled(
             blink::features::kPartialLowEndModeExcludeCanvasFontCache);
#else
  return IsLowEndDeviceOrPartialLowEndModeEnabled();
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
    base::MemoryPressureLevel level) {
  TRACE_EVENT1("blink", "MemoryPressureListenerRegistry::onMemoryPressure",
               "level", level);
  CHECK(IsMainThread());
  for (auto& client : clients_)
    client->OnMemoryPressure(level);
}

void MemoryPressureListenerRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(clients_);
}

}  // namespace blink
