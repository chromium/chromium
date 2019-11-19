// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WORKER_NAVIGATOR_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WORKER_NAVIGATOR_WAKE_LOCK_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScriptState;
class WakeLock;
class WorkerNavigator;

class WorkerNavigatorWakeLock final
    : public GarbageCollected<WorkerNavigatorWakeLock>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorWakeLock);

 public:
  static const char kSupplementName[];

  static WorkerNavigatorWakeLock& From(WorkerNavigator&);

  static WakeLock* wakeLock(ScriptState*, WorkerNavigator&);

  explicit WorkerNavigatorWakeLock(WorkerNavigator&);

  void Trace(blink::Visitor*) override;

 private:
  WakeLock* GetWakeLock(ScriptState*);

  Member<WakeLock> wake_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WORKER_NAVIGATOR_WAKE_LOCK_H_
