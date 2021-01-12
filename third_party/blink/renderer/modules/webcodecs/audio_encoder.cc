// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

namespace blink {

AudioEncoder* AudioEncoder::Create(ScriptState* script_state,
                                   const AudioEncoderInit* init,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioEncoder>(script_state, init,
                                            exception_state);
}

AudioEncoder::AudioEncoder(ScriptState* script_state,
                           const AudioEncoderInit* init,
                           ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)) {}

AudioEncoder::~AudioEncoder() = default;

int32_t AudioEncoder::encodeQueueSize() {
  return 0;
}

void AudioEncoder::encode(AudioFrame* frame, ExceptionState&) {}

void AudioEncoder::configure(const AudioEncoderConfig*, ExceptionState&) {}

ScriptPromise AudioEncoder::flush(ExceptionState&) {
  return ScriptPromise();
}

void AudioEncoder::reset(ExceptionState&) {}

void AudioEncoder::close(ExceptionState&) {}

String AudioEncoder::state() {
  return "";
}

void AudioEncoder::ContextDestroyed() {}

void AudioEncoder::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
