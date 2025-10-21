// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_

#include "base/memory/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT MemoryPressureListener : public GarbageCollectedMixin {
 public:
  virtual ~MemoryPressureListener() = default;

  virtual void OnMemoryPressure(base::MemoryPressureLevel) {}
};

// MemoryPressureListenerRegistry listens to some events which could be
// opportunities for reducing memory consumption and notifies its clients.
class PLATFORM_EXPORT MemoryPressureListenerRegistry final
    : public GarbageCollected<MemoryPressureListenerRegistry> {
 public:
  static MemoryPressureListenerRegistry& Instance();

  // See: SysUtils::IsLowEndDevice for the full details of what "low-end" means.
  // This returns true for devices that can use more extreme tradeoffs for
  // performance. Many low memory devices (<=1GB) are not considered low-end.
  // Can be overridden in web tests via internals.
  static bool IsLowEndDevice();

  // Returns true when IsLowEndDevice() returns true or when the feature
  // PartialLowEndModeOnMidEndDevices is enabled on Android devices.
  static bool IsLowEndDeviceOrPartialLowEndModeEnabled();

  // Returns true when IsLowEndDevice() Or PartialLowEndModeOnMidEndDevices is
  // enabled and canvas font cache is not excluded from the features.
  static bool
  IsLowEndDeviceOrPartialLowEndModeEnabledIncludingCanvasFontCache();

  // Caches whether this device is a low-end device and the device physical
  // memory in static members. instance() is not used as it's a heap allocated
  // object - meaning it's not thread-safe as well as might break tests counting
  // the heap size.
  static void Initialize();

  MemoryPressureListenerRegistry();
  MemoryPressureListenerRegistry(const MemoryPressureListenerRegistry&) =
      delete;
  MemoryPressureListenerRegistry& operator=(
      const MemoryPressureListenerRegistry&) = delete;

  // RegisterClient() and UnregisterClient() work only in the main thread.
  void RegisterClient(MemoryPressureListener*);
  void UnregisterClient(MemoryPressureListener*);

  void OnMemoryPressure(base::MemoryPressureLevel);

  void Trace(Visitor*) const;

 private:
  friend class Internals;

  static void SetIsLowEndDeviceForTesting(bool);

  static bool is_low_end_device_;

  HeapHashSet<WeakMember<MemoryPressureListener>> clients_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
