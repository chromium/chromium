// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_APP_DELEGATE_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_APP_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

namespace blink {
namespace mojom {
class FileChooserParams;
class WindowFeatures;
}
}  // namespace blink

namespace content {
enum class PictureInPictureResult;
class BrowserContext;
class FileSelectListener;
class NavigationHandle;
class RenderFrameHost;
class WebContents;
struct OpenURLParams;
}

namespace gfx {
class Rect;
class Size;
}

namespace extensions {

class Extension;

// Interface to give packaged apps access to services in the browser, for things
// like handling links and showing UI prompts to the user.
class AppDelegate {
 public:
  virtual ~AppDelegate() {}

  // General initialization.
  virtual void InitWebContents(content::WebContents* web_contents) = 0;
  virtual void RenderFrameCreated(content::RenderFrameHost* frame_host) = 0;

  // Resizes WebContents.
  virtual void ResizeWebContents(content::WebContents* web_contents,
                                 const gfx::Size& size) = 0;

  // Link handling.
  virtual content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) = 0;
  virtual void AddNewContents(
      content::BrowserContext* context,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture) = 0;

  // Feature support.
  virtual void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params) = 0;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const Extension* extension) = 0;
  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type,
      const Extension* extension) = 0;
  virtual int PreferredIconSize() const = 0;

  // Web contents modal dialog support.
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) = 0;
  virtual bool IsWebContentsVisible(content::WebContents* web_contents) = 0;

  // |callback| will be called when the process is about to terminate.
  virtual void SetTerminatingCallback(base::OnceClosure callback) = 0;

  // Called when the app is hidden or shown.
  virtual void OnHide() = 0;
  virtual void OnShow() = 0;

  // Called when app web contents finishes focus traversal - gives the delegate
  // a chance to handle the focus change.
  // Return whether focus has been handled.
  virtual bool TakeFocus(content::WebContents* web_contents, bool reverse) = 0;

  // Notifies the Picture-in-Picture controller that there is a new player
  // entering Picture-in-Picture.
  // Returns the result of the enter request.
  virtual content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) = 0;

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  virtual void ExitPictureInPicture() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_APP_DELEGATE_H_
