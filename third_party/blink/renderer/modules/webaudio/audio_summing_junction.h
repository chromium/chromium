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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SUMMING_JUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SUMMING_JUNCTION_H_

#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class AudioNodeOutput;
class DeferredTaskHandler;

// An AudioSummingJunction represents a point where zero, one, or more
// AudioNodeOutputs connect.

class AudioSummingJunction {
 public:
  virtual ~AudioSummingJunction();

  // Can be called from any thread.
  DeferredTaskHandler& GetDeferredTaskHandler() const {
    return *deferred_task_handler_;
  }

  // This must be called whenever we modify m_outputs.
  void ChangedOutputs();

  // This copies m_outputs to m_renderingOutputs. Please see comments for these
  // lists below.  This must be called when we own the context's graph lock in
  // the audio thread at the very start or end of the render quantum.
  void UpdateRenderingState();

  // Rendering code accesses its version of the current connections here.
  unsigned NumberOfRenderingConnections() const {
    return rendering_outputs_.size();
  }
  AudioNodeOutput* RenderingOutput(unsigned i) { return rendering_outputs_[i]; }
  bool IsConnected() const { return NumberOfRenderingConnections() > 0; }

  virtual void DidUpdate() = 0;

 protected:
  explicit AudioSummingJunction(DeferredTaskHandler&);

  scoped_refptr<DeferredTaskHandler> deferred_task_handler_;

  // m_outputs contains the AudioNodeOutputs representing current connections
  // which are not disabled.  The rendering code should never use this
  // directly, but instead uses m_renderingOutputs.
  // These raw pointers are safe. Owner AudioNodes of these AudioNodeOutputs
  // manage their lifetime, and AudioNode::dispose() disconnects all of
  // connections.
  HashSet<AudioNodeOutput*> outputs_;

  // m_renderingOutputs is a copy of m_outputs which will never be modified
  // during the graph rendering on the audio thread.  This is the list which
  // is used by the rendering code.
  // Whenever m_outputs is modified, the context is told so it can later
  // update m_renderingOutputs from m_outputs at a safe time.  Most of the
  // time, m_renderingOutputs is identical to m_outputs.
  // These raw pointers are safe. Owner of this AudioSummingJunction has
  // strong references to owners of these AudioNodeOutput.
  Vector<AudioNodeOutput*> rendering_outputs_;

  // m_renderingStateNeedUpdating keeps track if m_outputs is modified.
  bool rendering_state_need_updating_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_SUMMING_JUNCTION_H_
