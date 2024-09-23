// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FLINGING_CONTROLLER_H_
#define MEDIA_BASE_FLINGING_CONTROLLER_H_

#include "base/time/time.h"
#include "media/base/media_controller.h"
#include "media/base/media_status_observer.h"

namespace media {

// Interface that groups all the necessary hooks to control media that is being
// flung to a cast device, as part of RemotePlayback.
// TODO(crbug.com/41375562): Rename this interface to MediaRouteController
// and change media_router::MediaRouteController to MojoMediaRouteController.
class FlingingController {
 public:
  virtual ~FlingingController() = default;

  // Gets a MediaContoller owned by |this| to issue simple commands.
  virtual MediaController* GetMediaController() = 0;

  // Subscribe or un-subscribe to changes in the media status.
  virtual void AddMediaStatusObserver(MediaStatusObserver* observer) = 0;
  virtual void RemoveMediaStatusObserver(MediaStatusObserver* observer) = 0;

  // Gets the current media playback time. Implementers may sacrifice precision
  // to avoid a round-trip query to cast devices (see
  // RemoteMediaPlayer.getApproximateStreamPosition() for example).
  virtual base::TimeDelta GetApproximateCurrentTime() = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_FLINGING_CONTROLLER_H_
