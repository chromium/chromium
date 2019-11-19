// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_muting_session.h"

namespace content {

AudioMutingSession::AudioMutingSession(const base::UnguessableToken& group_id)
    : group_id_(group_id) {}

AudioMutingSession::~AudioMutingSession() = default;

void AudioMutingSession::Connect(audio::mojom::StreamFactory* factory) {
  if (muter_)
    muter_.reset();

  DCHECK(factory);
  factory->BindMuter(muter_.BindNewEndpointAndPassReceiver(), group_id_);
}

}  // namespace content
