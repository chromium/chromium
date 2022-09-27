// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/audio_output_device_controller.h"

namespace blink {

// static
const char AudioOutputDeviceController::kSupplementName[] =
    "AudioOutputDeviceController";

// static
AudioOutputDeviceController* AudioOutputDeviceController::From(
    HTMLMediaElement& element) {
  return Supplement<HTMLMediaElement>::From<AudioOutputDeviceController>(
      element);
}

void AudioOutputDeviceController::Trace(Visitor* visitor) const {
  Supplement<HTMLMediaElement>::Trace(visitor);
}

AudioOutputDeviceController::AudioOutputDeviceController(
    HTMLMediaElement& element)
    : Supplement<HTMLMediaElement>(element) {}

// static
void AudioOutputDeviceController::ProvideTo(
    HTMLMediaElement& element,
    AudioOutputDeviceController* controller) {
  Supplement<HTMLMediaElement>::ProvideTo(element, controller);
}

}  // namespace blink
