// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_stream_web_contents_observer.h"

#include <string>

#include "content/public/browser/browser_thread.h"

namespace content {

MediaStreamWebContentsObserver::MediaStreamWebContentsObserver(
    WebContents* web_contents,
    base::RepeatingClosure focus_callback)
    : WebContentsObserver(web_contents) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);

  on_focus_callback_ = std::move(focus_callback);
}

MediaStreamWebContentsObserver::~MediaStreamWebContentsObserver() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);

  on_focus_callback_.Reset();
  Observe(nullptr);
}

void MediaStreamWebContentsObserver::OnWebContentsFocused(
    RenderWidgetHost* render_widget_host) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);

  if (on_focus_callback_)
    on_focus_callback_.Run();
}

}  // namespace content
