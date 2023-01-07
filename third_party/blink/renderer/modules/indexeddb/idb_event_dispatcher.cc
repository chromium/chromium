/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/indexeddb/idb_event_dispatcher.h"

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DispatchEventResult IDBEventDispatcher::Dispatch(
    Event& event,
    HeapVector<Member<EventTarget>>& event_targets) {
  wtf_size_t size = event_targets.size();
  DCHECK(size);

  event.SetEventPhase(Event::PhaseType::kCapturingPhase);
  for (wtf_size_t i = size - 1; i; --i) {  // Don't do the first element.
    event.SetCurrentTarget(event_targets[i].Get());
    event_targets[i]->FireEventListeners(event);
    if (event.PropagationStopped())
      goto doneDispatching;
  }

  event.SetEventPhase(Event::PhaseType::kAtTarget);
  event.SetCurrentTarget(event_targets[0].Get());
  event_targets[0]->FireEventListeners(event);
  if (event.PropagationStopped() || !event.bubbles() || event.cancelBubble())
    goto doneDispatching;

  event.SetEventPhase(Event::PhaseType::kBubblingPhase);
  for (wtf_size_t i = 1; i < size; ++i) {  // Don't do the first element.
    event.SetCurrentTarget(event_targets[i].Get());
    event_targets[i]->FireEventListeners(event);
    if (event.PropagationStopped() || event.cancelBubble())
      goto doneDispatching;
  }

doneDispatching:
  event.SetCurrentTarget(nullptr);
  event.SetEventPhase(Event::PhaseType::kNone);
  return EventTarget::GetDispatchEventResult(event);
}

}  // namespace blink
