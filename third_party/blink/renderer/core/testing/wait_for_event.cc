// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/wait_for_event.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

WaitForEvent::WaitForEvent() = default;

WaitForEvent::WaitForEvent(EventTarget* target, const AtomicString& name) {
  AddEventListener(target, name);
  base::RunLoop run_loop;
  AddCompletionClosure(run_loop.QuitClosure());
  run_loop.Run();
}

void WaitForEvent::AddEventListener(EventTarget* target,
                                    const AtomicString& name) {
  target->addEventListener(name, this, /*use_capture=*/false);
}

void WaitForEvent::AddCompletionClosure(base::OnceClosure closure) {
  closures_.push_back(std::move(closure));
}

void WaitForEvent::Invoke(ExecutionContext*, Event* event) {
  event_ = event;

  auto listeners = std::move(listeners_);
  auto closures = std::move(closures_);
  for (const auto& [target, name] : listeners)
    target->removeEventListener(name, this, /*use_capture=*/false);
  for (auto& closure : closures)
    std::move(closure).Run();
}

void WaitForEvent::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(listeners_);
  visitor->Trace(event_);
}

}  // namespace blink
