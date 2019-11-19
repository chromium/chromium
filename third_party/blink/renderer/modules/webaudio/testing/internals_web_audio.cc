// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/testing/internals_web_audio.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

unsigned InternalsWebAudio::audioHandlerCount(Internals& internals) {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(
      stderr, "InternalsWebAudio::audioHandlerCount = %u\n",
      InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter));
#endif
  return InstanceCounters::CounterValue(InstanceCounters::kAudioHandlerCounter);
}

}  // namespace blink
