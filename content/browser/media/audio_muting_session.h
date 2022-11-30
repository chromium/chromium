// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_

#include "base/unguessable_token.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

class AudioMutingSession {
 public:
  explicit AudioMutingSession(const base::UnguessableToken& group_id);

  AudioMutingSession(const AudioMutingSession&) = delete;
  AudioMutingSession& operator=(const AudioMutingSession&) = delete;

  ~AudioMutingSession();

  void Connect(media::mojom::AudioStreamFactory* factory);

 private:
  const base::UnguessableToken group_id_;
  mojo::AssociatedRemote<media::mojom::LocalMuter> muter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
