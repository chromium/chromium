// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_screen_orientation.h"

#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
ScreenScreenOrientation& ScreenScreenOrientation::From(Screen& screen) {
  ScreenScreenOrientation* supplement =
      Supplement<Screen>::From<ScreenScreenOrientation>(screen);
  if (!supplement) {
    supplement = MakeGarbageCollected<ScreenScreenOrientation>();
    ProvideTo(screen, supplement);
  }
  return *supplement;
}

// static
ScreenOrientation* ScreenScreenOrientation::orientation(Screen& screen) {
  ScreenScreenOrientation& self = ScreenScreenOrientation::From(screen);
  if (!screen.GetFrame())
    return nullptr;

  if (!self.orientation_)
    self.orientation_ = ScreenOrientation::Create(screen.GetFrame());

  return self.orientation_;
}

const char ScreenScreenOrientation::kSupplementName[] =
    "ScreenScreenOrientation";

void ScreenScreenOrientation::Trace(blink::Visitor* visitor) {
  visitor->Trace(orientation_);
  Supplement<Screen>::Trace(visitor);
}

}  // namespace blink
