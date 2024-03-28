// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_WEB_CONTENTS_HELPER_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_WEB_CONTENTS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/media_stream_request.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace blink {
class WebGestureEvent;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
struct OpenURLParams;
class WebContents;
class NavigationHandle;
}  // namespace content

namespace extensions {

class AppDelegate;
class Extension;

// Provides common functionality for apps and launcher pages to respond to
// messages from a WebContents.
class AppWebContentsHelper {
 public:
  AppWebContentsHelper(content::BrowserContext* browser_context,
                       const ExtensionId& extension_id,
                       content::WebContents* web_contents,
                       AppDelegate* app_delegate);

  AppWebContentsHelper(const AppWebContentsHelper&) = delete;
  AppWebContentsHelper& operator=(const AppWebContentsHelper&) = delete;

  // Returns true if the given |event| should not be handled by the renderer.
  static bool ShouldSuppressGestureEvent(const blink::WebGestureEvent& event);

  // Opens a new URL inside the passed in WebContents. See WebContentsDelegate.
  content::WebContents* OpenURLFromTab(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) const;

  // Requests to lock the mouse pointer. See WebContentsDelegate.
  void RequestPointerLock() const;

  // Asks permission to use the camera and/or microphone. See
  // WebContentsDelegate.
  void RequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) const;

  // Checks permission to use the camera or microphone. See
  // WebContentsDelegate.
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) const;

 private:
  const Extension* GetExtension() const;

  // The browser context with which this window is associated.
  // AppWindowWebContentsDelegate does not own this object.
  raw_ptr<content::BrowserContext> browser_context_;

  const ExtensionId extension_id_;

  raw_ptr<content::WebContents> web_contents_;

  raw_ptr<AppDelegate> app_delegate_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_WEB_CONTENTS_HELPER_H_
