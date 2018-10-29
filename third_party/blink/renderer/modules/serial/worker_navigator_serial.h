// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_WORKER_NAVIGATOR_SERIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_WORKER_NAVIGATOR_SERIAL_H_

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScriptState;
class Serial;

class WorkerNavigatorSerial final
    : public GarbageCollected<WorkerNavigatorSerial>,
      public Supplement<WorkerNavigator> {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorSerial);

 public:
  static const char kSupplementName[];

  static WorkerNavigatorSerial& From(WorkerNavigator&);

  static Serial* serial(ScriptState*, WorkerNavigator&);
  Serial* serial(ScriptState*);

  void Trace(Visitor*) override;

 private:
  explicit WorkerNavigatorSerial(WorkerNavigator&);

  Member<Serial> serial_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_WORKER_NAVIGATOR_SERIAL_H_
