// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class WakeLock;

class NavigatorWakeLock final : public GarbageCollected<NavigatorWakeLock>,
                                public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorWakeLock);

 public:
  static const char kSupplementName[];

  static NavigatorWakeLock& From(Navigator&);

  static WakeLock* wakeLock(Navigator&);

  explicit NavigatorWakeLock(Navigator&);

  void Trace(blink::Visitor*) override;

 private:
  WakeLock* GetWakeLock();

  Member<WakeLock> wake_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_NAVIGATOR_WAKE_LOCK_H_
