// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_HOST_DELEGATE_H_
#define EXTENSIONS_BROWSER_EXTENSION_HOST_DELEGATE_H_

#include <string>

#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/base/window_open_disposition.h"

namespace content {
enum class PictureInPictureResult;
class JavaScriptDialogManager;
class RenderFrameHost;
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace viz {
class SurfaceId;
}

namespace extensions {
class Extension;
class ExtensionHost;
class ExtensionHostQueue;

// A delegate to support functionality that cannot exist in the extensions
// module. This is not an inner class of ExtensionHost to allow it to be forward
// declared.
class ExtensionHostDelegate {
 public:
  virtual ~ExtensionHostDelegate() {}

  // Called after the hosting |web_contents| for an extension is created. The
  // implementation may wish to add preference observers to |web_contents|.
  virtual void OnExtensionHostCreated(content::WebContents* web_contents) = 0;

  // Called after |host| creates a RenderView for an extension.
  virtual void OnRenderViewCreatedForBackgroundPage(ExtensionHost* host) = 0;

  // Returns the embedder's JavaScriptDialogManager or NULL if the embedder
  // does not support JavaScript dialogs.
  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager() = 0;

  // Creates a new tab or popup window with |web_contents|. The embedder may
  // choose to do nothing if tabs and popups are not supported.
  virtual void CreateTab(std::unique_ptr<content::WebContents> web_contents,
                         const std::string& extension_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_rect,
                         bool user_gesture) = 0;

  // Requests access to an audio or video media stream. Invokes |callback|
  // with the response.
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const Extension* extension) = 0;

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
      blink::mojom::MediaStreamType type,
      const Extension* extension) = 0;

  // Returns the ExtensionHostQueue implementation to use for creating
  // ExtensionHost renderers.
  virtual ExtensionHostQueue* GetExtensionHostQueue() const = 0;

  // Notifies the Picture-in-Picture controller that there is a new player
  // entering Picture-in-Picture.
  // Returns the result of the enter request.
  virtual content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size) = 0;

  // Updates the Picture-in-Picture controller with a signal that
  // Picture-in-Picture mode has ended.
  virtual void ExitPictureInPicture() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_HOST_DELEGATE_H_
