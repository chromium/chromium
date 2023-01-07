// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/webaudio/audio_basic_processor_handler.h"

namespace blink {

class AudioNode;
class AudioParamHandler;

class DelayHandler : public AudioBasicProcessorHandler {
 public:
  static scoped_refptr<DelayHandler> Create(AudioNode&,
                                            float sample_rate,
                                            AudioParamHandler& delay_time,
                                            double max_delay_time);

 private:
  DelayHandler(AudioNode&,
               float sample_rate,
               AudioParamHandler& delay_time,
               double max_delay_time);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_DELAY_HANDLER_H_
