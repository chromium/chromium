// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_SESSION_CREATION_OBSERVER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_SESSION_CREATION_OBSERVER_WIN_H_

#include <audiopolicy.h>
#include <wrl/implements.h>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace media {

// Calls the given callback when notified by the IAudioSessionManager2 that an
// audio session was created.
class MEDIA_EXPORT AudioSessionCreationObserverWin
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAudioSessionNotification> {
 public:
  // `session_created_callback` is called when the system notifies us of a new
  // audio session via `OnSessionCreated()`.
  explicit AudioSessionCreationObserverWin(
      base::RepeatingClosure session_created_callback);
  AudioSessionCreationObserverWin(const AudioSessionCreationObserverWin&) =
      delete;
  AudioSessionCreationObserverWin& operator=(
      const AudioSessionCreationObserverWin&) = delete;
  ~AudioSessionCreationObserverWin() override;

  // IAudioSessionNotification:
  IFACEMETHODIMP OnSessionCreated(IAudioSessionControl* session) override;

 private:
  base::RepeatingClosure session_created_cb_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_SESSION_CREATION_OBSERVER_WIN_H_
