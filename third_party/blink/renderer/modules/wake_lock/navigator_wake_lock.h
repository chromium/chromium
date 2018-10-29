// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;

class NavigatorWakeLock final : public GarbageCollected<NavigatorWakeLock>,
                                public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorWakeLock);

 public:
  static const char kSupplementName[];

  static NavigatorWakeLock& From(Navigator&);

  static ScriptPromise getWakeLock(ScriptState*, Navigator&, String);

  void Trace(blink::Visitor*) override;

 private:
  ScriptPromise getWakeLock(ScriptState*, String);
  explicit NavigatorWakeLock(Navigator&);

  Member<WakeLock> wake_lock_screen_;
  Member<WakeLock> wake_lock_system_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_
