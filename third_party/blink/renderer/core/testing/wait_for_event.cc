// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/wait_for_event.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

WaitForEvent::WaitForEvent(Element* element, const AtomicString& name)
    : element_(element), event_name_(name) {
  element_->addEventListener(event_name_, this);
  run_loop_.Run();
}

void WaitForEvent::Invoke(ExecutionContext*, Event*) {
  run_loop_.Quit();
  element_->removeEventListener(event_name_, this);
}

void WaitForEvent::Trace(Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  visitor->Trace(element_);
}

}  // namespace blink
