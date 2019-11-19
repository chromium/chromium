// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_SCREEN_ORIENTATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_SCREEN_ORIENTATION_H_

#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScreenOrientation;
class Screen;

class ScreenScreenOrientation final
    : public GarbageCollected<ScreenScreenOrientation>,
      public Supplement<Screen> {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenScreenOrientation);

 public:
  static const char kSupplementName[];

  static ScreenScreenOrientation& From(Screen&);

  static ScreenOrientation* orientation(Screen&);

  void Trace(blink::Visitor*) override;

 private:
  Member<ScreenOrientation> orientation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_SCREEN_ORIENTATION_H_
