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

#include "third_party/blink/renderer/modules/webaudio/audio_summing_junction.h"

#include <algorithm>

#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"

namespace blink {

AudioSummingJunction::AudioSummingJunction(DeferredTaskHandler& handler)
    : deferred_task_handler_(&handler) {}

AudioSummingJunction::~AudioSummingJunction() {
  GetDeferredTaskHandler().AssertGraphOwner();
  GetDeferredTaskHandler().RemoveMarkedSummingJunction(this);
}

void AudioSummingJunction::ChangedOutputs() {
  GetDeferredTaskHandler().AssertGraphOwner();
  if (!rendering_state_need_updating_) {
    GetDeferredTaskHandler().MarkSummingJunctionDirty(this);
    rendering_state_need_updating_ = true;
  }
}

void AudioSummingJunction::UpdateRenderingState() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  GetDeferredTaskHandler().AssertGraphOwner();
  if (rendering_state_need_updating_) {
    // Copy from `outputs_` to `rendering_outputs_`.
    rendering_outputs_.resize(outputs_.size());
    unsigned j = 0;
    for (AudioNodeOutput* output : outputs_) {
      rendering_outputs_[j++] = output;
      output->UpdateRenderingState();
    }

    DidUpdate();

    rendering_state_need_updating_ = false;
  }
}

}  // namespace blink
