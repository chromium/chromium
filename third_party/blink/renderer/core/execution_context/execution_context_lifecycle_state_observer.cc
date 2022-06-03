/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

ExecutionContextLifecycleStateObserver::ExecutionContextLifecycleStateObserver(
    ExecutionContext* execution_context)
    : ExecutionContextLifecycleObserver(execution_context, kStateObjectType) {
  DCHECK(!execution_context || execution_context->IsContextThread());
  InstanceCounters::IncrementCounter(
      InstanceCounters::kContextLifecycleStateObserverCounter);
}

ExecutionContextLifecycleStateObserver::
    ~ExecutionContextLifecycleStateObserver() {
  InstanceCounters::DecrementCounter(
      InstanceCounters::kContextLifecycleStateObserverCounter);

#if DCHECK_IS_ON()
  DCHECK(update_state_if_needed_called_);
#endif
}

void ExecutionContextLifecycleStateObserver::UpdateStateIfNeeded() {
#if DCHECK_IS_ON()
  DCHECK(!update_state_if_needed_called_);
  update_state_if_needed_called_ = true;
#endif
  if (ExecutionContext* context = GetExecutionContext()) {
#if DCHECK_IS_ON()
    DCHECK(context->ContextLifecycleObserverSet().HasObserver(this));
#endif
    mojom::blink::FrameLifecycleState pause_state =
        context->ContextPauseState();
    if (pause_state != mojom::blink::FrameLifecycleState::kRunning)
      ContextLifecycleStateChanged(pause_state);
  }
}

void ExecutionContextLifecycleStateObserver::SetExecutionContext(
    ExecutionContext* context) {
  ExecutionContextLifecycleObserver::SetExecutionContext(context);
  if (!context)
    return;

  if (context->IsContextDestroyed()) {
    ContextDestroyed();
    return;
  }
  ContextLifecycleStateChanged(context->ContextPauseState());
}

}  // namespace blink
