/*
 * Copyright (C) 2012, Intel Corporation. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"

namespace blink {

AudioBasicInspectorHandler::AudioBasicInspectorHandler(NodeType node_type,
                                                       AudioNode& node,
                                                       float sample_rate)
    : AudioHandler(node_type, node, sample_rate), need_automatic_pull_(false) {
  AddInput();
}

// We override pullInputs() as an optimization allowing this node to take
// advantage of in-place processing, where the input is simply passed through
// unprocessed to the output.
// Note: this only applies if the input and output channel counts match.
void AudioBasicInspectorHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - try to render directly into output bus for
  // pass-through processing where process() doesn't need to do anything...
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

void AudioBasicInspectorHandler::CheckNumberOfChannelsForInput(
    AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &this->Input(0));

  unsigned number_of_channels = input->NumberOfChannels();

  if (number_of_channels != Output(0).NumberOfChannels()) {
    // This will propagate the channel count to any nodes connected further
    // downstream in the graph.
    Output(0).SetNumberOfChannels(number_of_channels);
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);

  UpdatePullStatusIfNeeded();
}

void AudioBasicInspectorHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  if (Output(0).IsConnected()) {
    // When an AudioBasicInspectorNode is connected to a downstream node, it
    // will get pulled by the downstream node, thus remove it from the context's
    // automatic pull list.
    if (need_automatic_pull_) {
      Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
      need_automatic_pull_ = false;
    }
  } else {
    unsigned number_of_input_connections =
        Input(0).NumberOfRenderingConnections();
    if (number_of_input_connections && !need_automatic_pull_) {
      // When an AudioBasicInspectorNode is not connected to any downstream node
      // while still connected from upstream node(s), add it to the context's
      // automatic pull list.
      Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
      need_automatic_pull_ = true;
    } else if (!number_of_input_connections && need_automatic_pull_) {
      // The AudioBasicInspectorNode is connected to nothing and is
      // not an AnalyserNode, remove it from the context's automatic
      // pull list.  AnalyserNode's need to be pulled even with no
      // inputs so that the internal state gets updated to hold the
      // right time and FFT data.
      Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
      need_automatic_pull_ = false;
    }
  }
}

}  // namespace blink
