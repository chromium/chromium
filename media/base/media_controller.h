// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_CONTROLLER_H_
#define MEDIA_BASE_MEDIA_CONTROLLER_H_

#include "base/time/time.h"

namespace media {

// High level interface that allows a controller to issue simple media commands.
// Modeled after the media_router.mojom.MediaController interface.
// State changes will be signaled via the MediaStatusObserver interface.
// TODO(tguilbert): Add MediaStatusObserver interface.
class MediaController {
 public:
  virtual ~MediaController() = default;

  // Starts playing the media if it is paused. Is a no-op if not supported by
  // the media or the media is already playing.
  virtual void Play() = 0;

  // Pauses the media if it is playing. Is a no-op if not supported by the media
  // or the media is already paused.
  virtual void Pause() = 0;

  // Mutes the media if |mute| is true, and unmutes it if false. Is a no-op if
  // not supported by the media.
  virtual void SetMute(bool mute) = 0;

  // Changes the current volume of the media, with 1 being the highest and 0
  // being the lowest/no sound. Does not change the (un)muted state of the
  // media. Is a no-op if not supported by the media.
  virtual void SetVolume(float volume) = 0;

  // Sets the current playback position. |time| must be less than or equal to
  // the duration of the media. Is a no-op if the media doesn't support seeking.
  virtual void Seek(base::TimeDelta time) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_CONTROLLER_H_
