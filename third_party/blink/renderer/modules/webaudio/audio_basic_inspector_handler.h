// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_HANDLER_H_

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"

namespace blink {

class AudioNodeInput;

// AudioBasicInspectorNode is an AudioNode with one input and possibly one
// output where the output might not necessarily connect to another node's
// input.  (It is up to the subclasses to create the output, if needed.)  If the
// output is not connected to any other node, then the AudioBasicInspectorNode's
// processIfNecessary() function will be called automatically by
// BaseAudioContext before the end of each render quantum so that it can inspect
// the audio stream.
class AudioBasicInspectorHandler : public AudioHandler {
 public:
  AudioBasicInspectorHandler(NodeType, AudioNode&, float sample_rate);

  // AudioHandler
  void PullInputs(uint32_t frames_to_process) override;
  void CheckNumberOfChannelsForInput(AudioNodeInput*) override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  void UpdatePullStatusIfNeeded() override;

 protected:
  // When setting to true, AudioBasicInspectorHandler will be pulled
  // automatically by BaseAudioContext before the end of each render quantum.
  bool need_automatic_pull_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_BASIC_INSPECTOR_HANDLER_H_
