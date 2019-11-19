// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_

#include <utility>

#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace content {

class CONTENT_EXPORT AudioMutingSession {
 public:
  explicit AudioMutingSession(const base::UnguessableToken& group_id);
  ~AudioMutingSession();

  void Connect(audio::mojom::StreamFactory* factory);

 private:
  const base::UnguessableToken group_id_;
  mojo::AssociatedRemote<audio::mojom::LocalMuter> muter_;

  DISALLOW_COPY_AND_ASSIGN(AudioMutingSession);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_MUTING_SESSION_H_
