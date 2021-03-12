// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_CONTENT_WATCHER_H_
#define EXTENSIONS_RENDERER_CONTENT_WATCHER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {
class WebString;
}

namespace content {
class RenderFrame;
}

namespace extensions {

// Handles watching the content of WebFrames to notify extensions when they
// match various patterns. This class tracks the set of relevant patterns (set
// by the WatchPages Mojo method) and the set that match on each WebFrame, and
// sends a ExtensionHostMsg_OnWatchedPageChange whenever a RenderFrame's set
// changes.
class ContentWatcher {
 public:
  ContentWatcher();
  ~ContentWatcher();

  // Handler for the WatchPages Mojo method in extensions.mojom.Renderer
  // interface.
  void OnWatchPages(const std::vector<std::string>& css_selectors);

  void OnRenderFrameCreated(content::RenderFrame* render_frame);

 private:
  // If any of these selectors match on a page, we need to send an
  // ExtensionHostMsg_OnWatchedPageChange back to the browser.
  blink::WebVector<blink::WebString> css_selectors_;

  DISALLOW_COPY_AND_ASSIGN(ContentWatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_CONTENT_WATCHER_H_
