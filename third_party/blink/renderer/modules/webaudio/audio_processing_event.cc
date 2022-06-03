/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/audio_processing_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_processing_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

AudioProcessingEvent* AudioProcessingEvent::Create() {
  return MakeGarbageCollected<AudioProcessingEvent>();
}

AudioProcessingEvent* AudioProcessingEvent::Create(AudioBuffer* input_buffer,
                                                   AudioBuffer* output_buffer,
                                                   double playback_time) {
  return MakeGarbageCollected<AudioProcessingEvent>(input_buffer, output_buffer,
                                                    playback_time);
}

AudioProcessingEvent* AudioProcessingEvent::Create(
    const AtomicString& type,
    const AudioProcessingEventInit* initializer) {
  return MakeGarbageCollected<AudioProcessingEvent>(type, initializer);
}

AudioProcessingEvent::AudioProcessingEvent() = default;

AudioProcessingEvent::AudioProcessingEvent(AudioBuffer* input_buffer,
                                           AudioBuffer* output_buffer,
                                           double playback_time)
    : Event(event_type_names::kAudioprocess, Bubbles::kYes, Cancelable::kNo),
      input_buffer_(input_buffer),
      output_buffer_(output_buffer),
      playback_time_(playback_time) {}

AudioProcessingEvent::AudioProcessingEvent(
    const AtomicString& type,
    const AudioProcessingEventInit* initializer)
    : Event(type, initializer) {
  input_buffer_ = initializer->inputBuffer();
  output_buffer_ = initializer->outputBuffer();
  playback_time_ = initializer->playbackTime();
}

AudioProcessingEvent::~AudioProcessingEvent() = default;

const AtomicString& AudioProcessingEvent::InterfaceName() const {
  return event_interface_names::kAudioProcessingEvent;
}

void AudioProcessingEvent::Trace(Visitor* visitor) const {
  visitor->Trace(input_buffer_);
  visitor->Trace(output_buffer_);
  Event::Trace(visitor);
}

}  // namespace blink
