// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/util.h"

#include "services/media_session/public/mojom/constants.mojom.h"

namespace {

constexpr base::TimeDelta kDefaultSeekTime =
    base::Seconds(media_session::mojom::kDefaultSeekTimeSeconds);

}  // namespace

namespace media_session {

void PerformMediaSessionAction(
    mojom::MediaSessionAction action,
    const mojo::Remote<mojom::MediaController>& media_controller_remote) {
  switch (action) {
    case mojom::MediaSessionAction::kPreviousTrack:
      media_controller_remote->PreviousTrack();
      break;
    case mojom::MediaSessionAction::kSeekBackward:
      media_controller_remote->Seek(kDefaultSeekTime * -1);
      break;
    case mojom::MediaSessionAction::kPlay:
      media_controller_remote->Resume();
      break;
    case mojom::MediaSessionAction::kPause:
      media_controller_remote->Suspend();
      break;
    case mojom::MediaSessionAction::kSeekForward:
      media_controller_remote->Seek(kDefaultSeekTime);
      break;
    case mojom::MediaSessionAction::kNextTrack:
      media_controller_remote->NextTrack();
      break;
    case mojom::MediaSessionAction::kStop:
      media_controller_remote->Stop();
      break;
    case mojom::MediaSessionAction::kEnterPictureInPicture:
      media_controller_remote->EnterPictureInPicture();
      break;
    case mojom::MediaSessionAction::kExitPictureInPicture:
      media_controller_remote->ExitPictureInPicture();
      break;
    case mojom::MediaSessionAction::kToggleMicrophone:
      media_controller_remote->ToggleMicrophone();
      break;
    case mojom::MediaSessionAction::kToggleCamera:
      media_controller_remote->ToggleCamera();
      break;
    case mojom::MediaSessionAction::kHangUp:
      media_controller_remote->HangUp();
      break;
    case mojom::MediaSessionAction::kRaise:
      media_controller_remote->Raise();
      break;
    case mojom::MediaSessionAction::kEnterAutoPictureInPicture:
      media_controller_remote->EnterAutoPictureInPicture();
      break;
    case mojom::MediaSessionAction::kSkipAd:
      media_controller_remote->SkipAd();
      break;
    case mojom::MediaSessionAction::kSetMute:
    case mojom::MediaSessionAction::kSeekTo:
    case mojom::MediaSessionAction::kScrubTo:
    case mojom::MediaSessionAction::kSwitchAudioDevice:
    case mojom::MediaSessionAction::kPreviousSlide:
    case mojom::MediaSessionAction::kNextSlide:
      break;
  }
}

}  // namespace media_session
