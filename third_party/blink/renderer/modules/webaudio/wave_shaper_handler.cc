// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/wave_shaper_handler.h"

#include <memory>

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_processor.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfChannels = 1;

}  // namespace

WaveShaperHandler::WaveShaperHandler(AudioNode& node, float sample_rate)
    : AudioBasicProcessorHandler(
          kNodeTypeWaveShaper,
          node,
          sample_rate,
          std::make_unique<WaveShaperProcessor>(
              sample_rate,
              kNumberOfChannels,
              node.context()->GetDeferredTaskHandler().RenderQuantumFrames())) {
  Initialize();
}

scoped_refptr<WaveShaperHandler> WaveShaperHandler::Create(AudioNode& node,
                                                           float sample_rate) {
  return base::AdoptRef(new WaveShaperHandler(node, sample_rate));
}

}  // namespace blink
