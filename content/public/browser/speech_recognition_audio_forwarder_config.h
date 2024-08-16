// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_AUDIO_FORWARDER_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_AUDIO_FORWARDER_CONFIG_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

struct CONTENT_EXPORT SpeechRecognitionAudioForwarderConfig {
  SpeechRecognitionAudioForwarderConfig(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate);
  SpeechRecognitionAudioForwarderConfig(
      SpeechRecognitionAudioForwarderConfig& other);
  ~SpeechRecognitionAudioForwarderConfig();

  mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
      audio_forwarder;

  // The number of channels of the audio send via the audio forwarder. This can
  // be any positive integer; however, the Speech On-Device API (SODA) will only
  // transcribe audio from the first channel. If audio transcription from the
  // other channels is required, the multi-channel audio should be mixed into a
  // monaural stream in the renderer before it's forwarded.
  int channel_count;

  // The sample rate of the audio. Must be a positive integer.
  int sample_rate;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_AUDIO_FORWARDER_CONFIG_H_
