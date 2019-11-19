// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebContents;

// Used in tests to wait for media in a WebContents to start or stop playing.
class MediaStartStopObserver : public WebContentsObserver {
 public:
  enum class Type { kStart, kStop };

  MediaStartStopObserver(WebContents* web_contents, Type type);
  ~MediaStartStopObserver() override;

  // WebContentsObserver implementation.
  void MediaStartedPlaying(const MediaPlayerInfo& info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;

  void Wait();

 private:
  base::RunLoop run_loop_;
  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(MediaStartStopObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MEDIA_START_STOP_OBSERVER_H_
