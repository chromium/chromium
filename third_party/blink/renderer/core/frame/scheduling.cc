// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/scheduling.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/is_input_pending_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

bool Scheduling::isInputPending(ScriptState* script_state,
                                const IsInputPendingOptions* options) const {
  DCHECK(RuntimeEnabledFeatures::ExperimentalIsInputPendingEnabled(
      ExecutionContext::From(script_state)));
  DCHECK(options);

  auto* frame = LocalDOMWindow::From(script_state)->GetFrame();
  if (!frame)
    return false;

  auto* scheduler = ThreadScheduler::Current();
  auto info = scheduler->GetPendingUserInputInfo(options->includeContinuous());

  for (const auto& attribution : info) {
    if (frame->CanAccessEvent(attribution)) {
      return true;
    }
  }
  return false;
}

bool Scheduling::isFramePending() const {
  auto* scheduler = ThreadScheduler::Current();
  return scheduler->IsBeginMainFrameScheduled();
}

}  // namespace blink
