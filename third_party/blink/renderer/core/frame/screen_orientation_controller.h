// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_ORIENTATION_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_ORIENTATION_CONTROLLER_H_

#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class WebLockOrientationCallback;

// ScreenOrientationController allows to manipulate screen orientation in Blink
// outside of the screen_orientation/ modules. It is an interface that the
// module will implement and add a provider for.
// Callers of ScreenOrientationController::from() should always assume the
// returned pointer can be nullptr.
class CORE_EXPORT ScreenOrientationController
    : public GarbageCollected<ScreenOrientationController>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationController);

 public:
  static const char kSupplementName[];

  virtual ~ScreenOrientationController() = default;

  static ScreenOrientationController* From(LocalFrame&);

  virtual void NotifyOrientationChanged() = 0;

  virtual void lock(WebScreenOrientationLockType,
                    std::unique_ptr<WebLockOrientationCallback>) = 0;
  virtual void unlock() = 0;

  // Returns whether a lock() call was made without an unlock() call. Others
  // frames might have changed the lock state so this should only be used to
  // know whether the current frame made an attempt to lock without explicitly
  // unlocking.
  virtual bool MaybeHasActiveLock() const = 0;

  void Trace(blink::Visitor*) override;

 protected:
  explicit ScreenOrientationController(LocalFrame&);
  // To be called by an ScreenOrientationController to register its
  // implementation.
  static void ProvideTo(LocalFrame&, ScreenOrientationController*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_ORIENTATION_CONTROLLER_H_
