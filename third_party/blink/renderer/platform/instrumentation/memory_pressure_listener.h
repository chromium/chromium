// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/web_memory_pressure_level.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class Thread;

class PLATFORM_EXPORT MemoryPressureListener : public GarbageCollectedMixin {
 public:
  virtual ~MemoryPressureListener() = default;

  virtual void OnMemoryPressure(WebMemoryPressureLevel) {}

  virtual void OnPurgeMemory() {}
};

// MemoryPressureListenerRegistry listens to some events which could be
// opportunities for reducing memory consumption and notifies its clients.
class PLATFORM_EXPORT MemoryPressureListenerRegistry final
    : public GarbageCollected<MemoryPressureListenerRegistry> {
 public:
  static MemoryPressureListenerRegistry& Instance();

  // Whether the device Blink runs on is a low-end device.
  // Can be overridden in web tests via internals.
  static bool IsLowEndDevice();

  // Returns true when available memory is low.
  // This is not cheap and should not be called repeatedly.
  static bool IsCurrentlyLowMemory();

  // Caches whether this device is a low-end device and the device physical
  // memory in static members. instance() is not used as it's a heap allocated
  // object - meaning it's not thread-safe as well as might break tests counting
  // the heap size.
  static void Initialize();

  MemoryPressureListenerRegistry();

  void RegisterThread(Thread*) LOCKS_EXCLUDED(threads_mutex_);
  void UnregisterThread(Thread*) LOCKS_EXCLUDED(threads_mutex_);

  void RegisterClient(MemoryPressureListener*);
  void UnregisterClient(MemoryPressureListener*);

  void OnMemoryPressure(WebMemoryPressureLevel);

  void OnPurgeMemory();

  void Trace(blink::Visitor*);

 private:
  friend class Internals;

  static void SetIsLowEndDeviceForTesting(bool);

  static void ClearThreadSpecificMemory();

  static bool is_low_end_device_;

  HeapHashSet<WeakMember<MemoryPressureListener>> clients_;
  HashSet<Thread*> threads_ GUARDED_BY(threads_mutex_);
  Mutex threads_mutex_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureListenerRegistry);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_PRESSURE_LISTENER_H_
