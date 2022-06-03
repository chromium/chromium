// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_stream_web_contents_observer.h"

#include <string>

#include "content/public/browser/browser_thread.h"

namespace content {

MediaStreamWebContentsObserver::MediaStreamWebContentsObserver(
    int render_process_id,
    int render_frame_id)
    : WebContentsObserver(WebContents::FromRenderFrameHost(
          RenderFrameHost::FromID(render_process_id, render_frame_id))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

MediaStreamWebContentsObserver::~MediaStreamWebContentsObserver() = default;

void MediaStreamWebContentsObserver::RegisterFocusCallback(
    base::RepeatingClosure focus_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  on_focus_callback_ = std::move(focus_callback);
}

void MediaStreamWebContentsObserver::StopObserving() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  on_focus_callback_.Reset();
  Observe(nullptr);
}

void MediaStreamWebContentsObserver::OnWebContentsFocused(
    RenderWidgetHost* render_widget_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (on_focus_callback_)
    on_focus_callback_.Run();
}

}  // namespace content
