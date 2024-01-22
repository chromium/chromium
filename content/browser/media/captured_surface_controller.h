// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

// Encapsulates the permission state and logic associated with the Captured
// Surface Control API. Objects of this class live on the IO thread.
class CONTENT_EXPORT CapturedSurfaceController {
 public:
  using CapturedSurfaceControlResult =
      ::blink::mojom::CapturedSurfaceControlResult;

  CapturedSurfaceController(GlobalRenderFrameHostId capturer_rfh_id,
                            WebContentsMediaCaptureId captured_wc_id);

  CapturedSurfaceController(
      WebContentsMediaCaptureId captured_wc_id,
      std::unique_ptr<CapturedSurfaceControlPermissionManager>
          permission_manager);

  virtual ~CapturedSurfaceController();

  CapturedSurfaceController(const CapturedSurfaceController&) = delete;
  CapturedSurfaceController& operator=(const CapturedSurfaceController&) =
      delete;

  // Set the captured WebContents this controller is associated with.
  // This may be called with a null `WebContentsMediaCaptureId`.
  virtual void UpdateCaptureTarget(WebContentsMediaCaptureId captured_wc_id);

  // Produce a wheel event on the captured surface.
  virtual void SendWheel(
      blink::mojom::CapturedWheelActionPtr action,
      base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback);

  // Get the zoom level of the captured tab.
  virtual void GetZoomLevel(
      base::OnceCallback<void(
          std::optional<int> zoom_level,
          blink::mojom::CapturedSurfaceControlResult result)> reply_callback);

  // Set the zoom level of the captured tab.
  virtual void SetZoomLevel(
      int zoom_level,
      base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback);

 private:
  using PermissionResult =
      ::content::CapturedSurfaceControlPermissionManager::PermissionResult;

  // The ID of the captured tab.
  // * When `this` object is constructed, `captured_wc_id_` is set to a valid ID
  // of a valid tab.
  // * Later, through dynamic switching of the captured surface, the user might
  // start capturing something other than a tab, leading to this field being set
  // to the null ID.
  //
  // TODO(crbug.com/1511754): Whenever `captured_wc_id_` is recorded - either
  // set to its initial value or updated to a new value - pipe along from the UI
  // thread the knowledge of whether the capture-session is self-capture, record
  // that here, and use it on the IO thread to reject API calls until the user
  // invokes share-this-tab-instead to change self-capture into
  // other-tab-capture.
  WebContentsMediaCaptureId captured_wc_id_;

  std::unique_ptr<CapturedSurfaceControlPermissionManager> permission_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_
