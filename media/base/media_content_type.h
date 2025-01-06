// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_CONTENT_TYPE_H_
#define MEDIA_BASE_MEDIA_CONTENT_TYPE_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// The content type of a media player, which will be used by MediaSession to
// control its players.
enum class MediaContentType {
  // Type indicating that a player is persistent, which needs to take audio
  // focus to play.
  kPersistent,
  // Type indicating that a player only plays a transient sound.
  kTransient,
  // Type indicating that a player is a Pepper instance. MediaSession may duck
  // the player instead of pausing it.
  kPepper,
  // Type indicating that a player cannot be controlled. MediaSession will take
  // audio focus when the player joins but will not let it respond to audio
  // focus changes.
  kOneShot,
  // The maximum number of media content types.
  kMax = kOneShot,
};

// Utility function for deciding the MediaContentType of a player based on its
// duration.
MEDIA_EXPORT MediaContentType
DurationToMediaContentType(base::TimeDelta duration);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_CONTENT_TYPE_H_
