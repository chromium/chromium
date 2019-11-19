// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/scheduling.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/pending_user_input.h"
#include "third_party/blink/renderer/platform/scheduler/public/pending_user_input_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

bool Scheduling::isInputPending(ScriptState* script_state,
                                const Vector<String>& input_types) const {
  DCHECK(RuntimeEnabledFeatures::ExperimentalIsInputPendingEnabled(
      ExecutionContext::From(script_state)));

  if (!Platform::Current()->IsLockedToSite()) {
    // As we're interested in checking pending events for as many frames as we
    // can on the main thread, restrict the API to the case where all frames in
    // a process are part of the same site to avoid leaking cross-site inputs.
    ExecutionContext::From(script_state)
        ->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "isInputPending requires site-per-process (crbug.com/910421)."));
    return false;
  }

  auto* scheduler = ThreadScheduler::Current();
  auto input_info = scheduler->GetPendingUserInputInfo();
  if (input_types.size() == 0) {
    // If unspecified, return true if any input type is pending.
    return input_info.HasPendingInputType(
        scheduler::PendingUserInputType::kAny);
  }

  bool has_pending_input = false;
  for (const String& input_type_string : input_types) {
    const auto pending_input_type = scheduler::PendingUserInput::TypeFromString(
        AtomicString(input_type_string));
    if (pending_input_type == scheduler::PendingUserInputType::kNone) {
      StringBuilder message;
      message.Append("Unknown input event type \"");
      message.Append(input_type_string);
      message.Append("\". Skipping.");
      ExecutionContext::From(script_state)
          ->AddConsoleMessage(ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kWarning, message.ToString()));
    }

    if (!has_pending_input)
      has_pending_input |= input_info.HasPendingInputType(pending_input_type);
  }
  return has_pending_input;
}

bool Scheduling::isFramePending() const {
  auto* scheduler = ThreadScheduler::Current();
  return scheduler->IsBeginMainFrameScheduled();
}

}  // namespace blink
