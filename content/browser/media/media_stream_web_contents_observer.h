// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_STREAM_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_STREAM_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class MediaStreamWebContentsObserver final : public WebContentsObserver {
 public:
  MediaStreamWebContentsObserver(WebContents* web_contents,
                                 base::RepeatingClosure focus_callback);
  ~MediaStreamWebContentsObserver() override;

  // WebContentsObserver implementation.
  void OnWebContentsFocused(RenderWidgetHost* render_widget_host) override;

 private:
  base::RepeatingClosure on_focus_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_STREAM_WEB_CONTENTS_OBSERVER_H_
