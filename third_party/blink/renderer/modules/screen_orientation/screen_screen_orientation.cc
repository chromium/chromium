// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_screen_orientation.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
ScreenScreenOrientation& ScreenScreenOrientation::From(Screen& screen) {
  ScreenScreenOrientation* supplement = screen.GetScreenScreenOrientation();
  if (!supplement) {
    supplement = MakeGarbageCollected<ScreenScreenOrientation>(screen);
    screen.SetScreenScreenOrientation(supplement);
  }
  return *supplement;
}

// static
ScreenOrientation* ScreenScreenOrientation::orientation(Screen& screen) {
  ScreenScreenOrientation& self = ScreenScreenOrientation::From(screen);
  auto* window = To<LocalDOMWindow>(screen.GetExecutionContext());
  if (!window)
    return nullptr;

  if (!self.orientation_)
    self.orientation_ = ScreenOrientation::Create(window);

  return self.orientation_.Get();
}

ScreenScreenOrientation::ScreenScreenOrientation(Screen& screen)
    : screen_(screen) {}

void ScreenScreenOrientation::Trace(Visitor* visitor) const {
  visitor->Trace(orientation_);
  visitor->Trace(screen_);
}

}  // namespace blink
