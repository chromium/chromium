// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_log_factory.h"

#include <utility>

#include "content/browser/media/media_internals.h"

namespace content {

AudioLogFactory::AudioLogFactory() = default;
AudioLogFactory::~AudioLogFactory() = default;

void AudioLogFactory::CreateAudioLog(
    media::mojom::AudioLogComponent component,
    int32_t component_id,
    mojo::PendingReceiver<media::mojom::AudioLog> audio_log_receiver) {
  MediaInternals::GetInstance()->CreateMojoAudioLog(
      static_cast<media::AudioLogFactory::AudioComponent>(component),
      component_id, std::move(audio_log_receiver));
}

}  // namespace content
