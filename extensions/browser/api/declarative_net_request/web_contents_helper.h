// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_WEB_CONTENTS_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_WEB_CONTENTS_HELPER_H_

#include "content/public/browser/web_contents_observer.h"

namespace extensions {
namespace declarative_net_request {

class RulesetManager;

// A WebContentsObserver to route WebContents lifecycle events to the
// RulesetManager.
class WebContentsHelper : public content::WebContentsObserver {
 public:
  WebContentsHelper(content::WebContents* web_contents);
  ~WebContentsHelper() override;

  // WebContentsObserver overrides:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Non-owned pointer.
  RulesetManager* const ruleset_manager_ = nullptr;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_WEB_CONTENTS_HELPER_H_
