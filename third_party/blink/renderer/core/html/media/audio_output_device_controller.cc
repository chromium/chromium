// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/audio_output_device_controller.h"

namespace blink {

// static
AudioOutputDeviceController* AudioOutputDeviceController::From(
    HTMLMediaElement& element) {
  return element.GetAudioOutputDeviceController();
}

void AudioOutputDeviceController::Trace(Visitor* visitor) const {
  visitor->Trace(html_media_element_);
}

AudioOutputDeviceController::AudioOutputDeviceController(
    HTMLMediaElement& element)
    : html_media_element_(element) {}

// static
void AudioOutputDeviceController::ProvideTo(
    HTMLMediaElement& element,
    AudioOutputDeviceController* controller) {
  element.SetAudioOutputDeviceController(controller);
}

}  // namespace blink
