// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_source_parameters.h"

namespace media {

AudioSourceParameters::AudioSourceParameters() = default;
AudioSourceParameters::AudioSourceParameters(
    const base::UnguessableToken& session_id)
    : session_id(session_id) {}
AudioSourceParameters::AudioSourceParameters(
    const AudioSourceParameters& params) = default;
AudioSourceParameters::~AudioSourceParameters() = default;

AudioSourceParameters::ProcessingConfig::ProcessingConfig(
    base::UnguessableToken id,
    AudioProcessingSettings settings)
    : id(id), settings(settings) {
  DCHECK(!id.is_empty());
}

}  // namespace media
