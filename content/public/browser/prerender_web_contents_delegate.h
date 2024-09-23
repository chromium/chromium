// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/web_contents_delegate.h"

namespace content {

// This is used as the delegate of WebContents created for a new tab where
// prerendering runs. The delegate will be swapped with a proper one on
// prerender page activation.
class CONTENT_EXPORT PrerenderWebContentsDelegate : public WebContentsDelegate {
 public:
  PrerenderWebContentsDelegate() = default;
  ~PrerenderWebContentsDelegate() override = default;

  // WebContentsDelegate overrides.
  WebContents* OpenURLFromTab(WebContents* source,
                              const OpenURLParams& params,
                              base::OnceCallback<void(NavigationHandle&)>
                                  navigation_handle_callback) override;
  WebContents* AddNewContents(
      WebContents* source,
      std::unique_ptr<WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void ActivateContents(WebContents* contents) override;
  void LoadingStateChanged(WebContents* source,
                           bool should_show_loading_ui) override;
  void CloseContents(WebContents* source) override;
  bool ShouldSuppressDialogs(WebContents* source) override;
  bool ShouldFocusPageAfterCrash(WebContents* source) override;
  bool TakeFocus(WebContents* source, bool reverse) override;
  void WebContentsCreated(WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          WebContents* new_contents) override;
  bool CanEnterFullscreenModeForTab(RenderFrameHost* requesting_frame) override;
  void EnterFullscreenModeForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void FullscreenStateChangedForTab(
      RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(WebContents*) override;
  bool IsFullscreenForTabOrPending(const WebContents* web_contents) override;
  void OnDidBlockNavigation(
      WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  bool ShouldAllowRunningInsecureContent(WebContents* web_contents,
                                         bool allowed_per_prefs,
                                         const url::Origin& origin,
                                         const GURL& resource_url) override;
  PreloadingEligibility IsPrerender2Supported(
      WebContents& web_contents) override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
