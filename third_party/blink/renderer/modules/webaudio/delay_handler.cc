// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/delay_handler.h"

#include <memory>

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/delay_processor.h"

namespace blink {

namespace {

constexpr unsigned kNumberOfChannels = 1;

}  // namespace

DelayHandler::DelayHandler(AudioNode& node,
                           float sample_rate,
                           AudioParamHandler& delay_time,
                           double max_delay_time)
    : AudioBasicProcessorHandler(
          kNodeTypeDelay,
          node,
          sample_rate,
          std::make_unique<DelayProcessor>(
              sample_rate,
              kNumberOfChannels,
              node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
              delay_time,
              max_delay_time)) {
  Initialize();
}

scoped_refptr<DelayHandler> DelayHandler::Create(AudioNode& node,
                                                 float sample_rate,
                                                 AudioParamHandler& delay_time,
                                                 double max_delay_time) {
  return base::AdoptRef(
      new DelayHandler(node, sample_rate, delay_time, max_delay_time));
}

}  // namespace blink
