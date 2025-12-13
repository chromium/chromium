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

MemoryPressureListenerRegistration::MemoryPressureListenerRegistration(
    base::Location location,
    base::MemoryPressureListenerTag tag,
    base::MemoryPressureListener* listener)
    : registration_(std::in_place, location, tag, listener) {}

MemoryPressureListenerRegistration::~MemoryPressureListenerRegistration() {
  CHECK(!registration_);
}

void MemoryPressureListenerRegistration::Dispose() {
  registration_.reset();
}

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
}

// static
void MemoryPressureListenerRegistry::SetIsLowEndDeviceForTesting(
    bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

}  // namespace blink
