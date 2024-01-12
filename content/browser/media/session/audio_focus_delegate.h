// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_

#include <optional>

#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace content {

class MediaSessionImpl;

// AudioFocusDelegate is an interface abstracting audio focus handling for the
// MediaSession class.
class AudioFocusDelegate {
 public:
  // Factory method returning an implementation of AudioFocusDelegate.
  static std::unique_ptr<AudioFocusDelegate> Create(
      MediaSessionImpl* media_session);

  virtual ~AudioFocusDelegate() = default;

  enum class AudioFocusResult {
    kSuccess,
    kFailed,
    kDelayed,
  };

  virtual AudioFocusResult RequestAudioFocus(
      media_session::mojom::AudioFocusType audio_focus_type) = 0;
  virtual void AbandonAudioFocus() = 0;

  // Retrieves the current |AudioFocusType| for the associated |MediaSession|.
  virtual std::optional<media_session::mojom::AudioFocusType>
  GetCurrentFocusType() const = 0;

  // |MediaSession| should call this when it's state changes.
  virtual void MediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr&) = 0;

  // Retrieves the current request ID for the associated |MediaSession|.
  virtual const base::UnguessableToken& request_id() const = 0;

  // Inform the AudioFocusManager that this request ID will no longer be used.
  virtual void ReleaseRequestId() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_AUDIO_FOCUS_DELEGATE_H_
