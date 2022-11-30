// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_basic_inspector_handler.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"

namespace blink {

AudioBasicInspectorHandler::AudioBasicInspectorHandler(NodeType node_type,
                                                       AudioNode& node,
                                                       float sample_rate)
    : AudioHandler(node_type, node, sample_rate) {
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

  DCHECK_EQ(input, &Input(0));

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
