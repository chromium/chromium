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
  ScreenScreenOrientation* supplement =
      Supplement<Screen>::From<ScreenScreenOrientation>(screen);
  if (!supplement) {
    supplement = MakeGarbageCollected<ScreenScreenOrientation>(screen);
    ProvideTo(screen, supplement);
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

const char ScreenScreenOrientation::kSupplementName[] =
    "ScreenScreenOrientation";

ScreenScreenOrientation::ScreenScreenOrientation(Screen& screen)
    : Supplement(screen) {}

void ScreenScreenOrientation::Trace(Visitor* visitor) const {
  visitor->Trace(orientation_);
  Supplement<Screen>::Trace(visitor);
}

}  // namespace blink
