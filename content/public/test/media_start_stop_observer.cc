// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/media_start_stop_observer.h"

namespace content {

MediaStartStopObserver::MediaStartStopObserver(WebContents* web_contents,
                                               Type type)
    : WebContentsObserver(web_contents), type_(type) {}

MediaStartStopObserver::~MediaStartStopObserver() = default;

void MediaStartStopObserver::MediaStartedPlaying(const MediaPlayerInfo& info,
                                                 const MediaPlayerId& id) {
  if (type_ != Type::kStart)
    return;

  run_loop_.Quit();
}

void MediaStartStopObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  if (is_picture_in_picture && type_ != Type::kEnterPictureInPicture)
    return;

  if (!is_picture_in_picture && type_ != Type::kExitPictureInPicture)
    return;

  run_loop_.Quit();
}

void MediaStartStopObserver::MediaStoppedPlaying(
    const MediaPlayerInfo& info,
    const MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  if (type_ != Type::kStop)
    return;

  run_loop_.Quit();
}

void MediaStartStopObserver::Wait() {
  run_loop_.Run();
}

}  // namespace content
