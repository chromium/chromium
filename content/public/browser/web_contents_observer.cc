// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_observer.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_details.h"

namespace content {

WebContentsObserver::WebContentsObserver(WebContents* web_contents) {
  Observe(web_contents);
}

WebContentsObserver::WebContentsObserver() = default;

WebContentsObserver::~WebContentsObserver() {
  if (web_contents_) {
    static_cast<WebContentsImpl*>(web_contents_)->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());
}

WebContents* WebContentsObserver::web_contents() const {
  return web_contents_;
}

void WebContentsObserver::Observe(WebContents* web_contents) {
  if (web_contents == web_contents_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (web_contents_)
    static_cast<WebContentsImpl*>(web_contents_)->RemoveObserver(this);
  web_contents_ = web_contents;
  if (web_contents_) {
    static_cast<WebContentsImpl*>(web_contents_)->AddObserver(this);
  }
}

bool WebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  return false;
}

void WebContentsObserver::ResetWebContents() {
  static_cast<WebContentsImpl*>(web_contents_)->RemoveObserver(this);
  web_contents_ = nullptr;
}

}  // namespace content
