// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/scheduling.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_is_input_pending_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"

namespace blink {

const char Scheduling::kSupplementName[] = "Scheduling";

Scheduling* Scheduling::scheduling(Navigator& navigator) {
  Scheduling* supplement = Supplement<Navigator>::From<Scheduling>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<Scheduling>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

Scheduling::Scheduling(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

bool Scheduling::isInputPending(const IsInputPendingOptions* options) const {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  DCHECK(options);
  if (!window)
    return false;

  auto* scheduler = ThreadScheduler::Current();
  auto info = scheduler->ToMainThreadScheduler()->GetPendingUserInputInfo(
      options->includeContinuous());

  for (const auto& attribution : info) {
    if (window->GetFrame()->CanAccessEvent(attribution)) {
      return true;
    }
  }
  return false;
}

void Scheduling::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
