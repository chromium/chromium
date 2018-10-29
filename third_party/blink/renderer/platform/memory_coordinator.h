// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEMORY_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEMORY_COORDINATOR_H_

#include "third_party/blink/public/platform/web_memory_pressure_level.h"
#include "third_party/blink/public/platform/web_memory_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class PLATFORM_EXPORT MemoryCoordinatorClient : public GarbageCollectedMixin {
 public:
  virtual ~MemoryCoordinatorClient() = default;

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  virtual void OnMemoryPressure(WebMemoryPressureLevel) {}

  virtual void OnMemoryStateChange(MemoryState) {}

  virtual void OnPurgeMemory() {}
};

// MemoryCoordinator listens to some events which could be opportunities
// for reducing memory consumption and notifies its clients.
class PLATFORM_EXPORT MemoryCoordinator final
    : public GarbageCollectedFinalized<MemoryCoordinator> {
  WTF_MAKE_NONCOPYABLE(MemoryCoordinator);

 public:
  static MemoryCoordinator& Instance();

  // Whether the device Blink runs on is a low-end device.
  // Can be overridden in layout tests via internals.
  static bool IsLowEndDevice();

  // Returns true when available memory is low.
  // This is not cheap and should not be called repeatedly.
  // TODO(keishi): Remove when MemoryState is ready.
  static bool IsCurrentlyLowMemory();

  // Caches whether this device is a low-end device and the device physical
  // memory in static members. instance() is not used as it's a heap allocated
  // object - meaning it's not thread-safe as well as might break tests counting
  // the heap size.
  static void Initialize();

  void RegisterThread(Thread*) LOCKS_EXCLUDED(threads_mutex_);
  void UnregisterThread(Thread*) LOCKS_EXCLUDED(threads_mutex_);

  void RegisterClient(MemoryCoordinatorClient*);
  void UnregisterClient(MemoryCoordinatorClient*);

  // TODO(bashi): Deprecating. Remove this when MemoryPressureListener is
  // gone.
  void OnMemoryPressure(WebMemoryPressureLevel);

  void OnMemoryStateChange(MemoryState);

  void OnPurgeMemory();

  void Trace(blink::Visitor*);

 private:
  friend class Internals;

  static void SetIsLowEndDeviceForTesting(bool);

  MemoryCoordinator();

  void ClearMemory();
  static void ClearThreadSpecificMemory();

  static bool is_low_end_device_;

  HeapHashSet<WeakMember<MemoryCoordinatorClient>> clients_;
  HashSet<Thread*> threads_;
  Mutex threads_mutex_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEMORY_COORDINATOR_H_
