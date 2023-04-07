/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_scheduled_source_node.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

AudioScheduledSourceNode::AudioScheduledSourceNode(BaseAudioContext& context)
    : AudioNode(context), ActiveScriptWrappable<AudioScheduledSourceNode>({}) {}

AudioScheduledSourceHandler&
AudioScheduledSourceNode::GetAudioScheduledSourceHandler() const {
  return static_cast<AudioScheduledSourceHandler&>(Handler());
}

void AudioScheduledSourceNode::start(ExceptionState& exception_state) {
  start(0, exception_state);
}

void AudioScheduledSourceNode::start(double when,
                                     ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Start(when, exception_state);
}

void AudioScheduledSourceNode::stop(ExceptionState& exception_state) {
  stop(0, exception_state);
}

void AudioScheduledSourceNode::stop(double when,
                                    ExceptionState& exception_state) {
  GetAudioScheduledSourceHandler().Stop(when, exception_state);
}

EventListener* AudioScheduledSourceNode::onended() {
  return GetAttributeEventListener(event_type_names::kEnded);
}

void AudioScheduledSourceNode::setOnended(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kEnded, listener);
}

bool AudioScheduledSourceNode::HasPendingActivity() const {
  // To avoid the leak, a node should be collected regardless of its
  // playback state if the context is closed.
  if (context()->ContextState() == BaseAudioContext::kClosed) {
    return false;
  }

  // If a node is scheduled or playing, do not collect the node
  // prematurely even its reference is out of scope. If the onended
  // event has not yet fired, we still have activity pending too.
  return ContainsHandler() &&
         (GetAudioScheduledSourceHandler().IsPlayingOrScheduled() ||
          GetAudioScheduledSourceHandler().IsOnEndedNotificationPending());
}

}  // namespace blink
