// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_WEB_CONTENTS_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

class MediaPlayerRenderer;

// This class propagates WebContents muting updates to MediaPlayerRenderers.
// This allows us to avoid adding N WebContentsObservers for N
// MediaPlayerRenderers on a page. Essentially, this is a call-stack filter to
// prevent uninteresting observer methods from calling into the
// MediaPlayerRenderers.
class MediaPlayerRendererWebContentsObserver
    : public WebContentsObserver,
      public WebContentsUserData<MediaPlayerRendererWebContentsObserver> {
 public:
  MediaPlayerRendererWebContentsObserver(
      const MediaPlayerRendererWebContentsObserver&) = delete;
  MediaPlayerRendererWebContentsObserver& operator=(
      const MediaPlayerRendererWebContentsObserver&) = delete;

  ~MediaPlayerRendererWebContentsObserver() override;

  void AddMediaPlayerRenderer(MediaPlayerRenderer* player);
  void RemoveMediaPlayerRenderer(MediaPlayerRenderer* player);

  // WebContentsObserver implementation.
  void DidUpdateAudioMutingState(bool muted) override;
  void WebContentsDestroyed() override;

 private:
  explicit MediaPlayerRendererWebContentsObserver(WebContents* web_contents);
  friend class WebContentsUserData<MediaPlayerRendererWebContentsObserver>;

  base::flat_set<raw_ptr<MediaPlayerRenderer, CtnExperimental>> players_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_WEB_CONTENTS_OBSERVER_H_
