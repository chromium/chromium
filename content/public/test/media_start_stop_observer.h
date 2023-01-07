// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebContents;

// Used in tests to wait for media in a WebContents to start or stop playing.
class MediaStartStopObserver : public WebContentsObserver {
 public:
  enum class Type {
    kStart,
    kStop,
    kEnterPictureInPicture,
    kExitPictureInPicture
  };

  MediaStartStopObserver(WebContents* web_contents, Type type);

  MediaStartStopObserver(const MediaStartStopObserver&) = delete;
  MediaStartStopObserver& operator=(const MediaStartStopObserver&) = delete;

  ~MediaStartStopObserver() override;

  // WebContentsObserver implementation.
  void MediaStartedPlaying(const MediaPlayerInfo& info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;

  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;

  void Wait();

 private:
  base::RunLoop run_loop_;
  const Type type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_
