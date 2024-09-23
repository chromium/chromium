// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

// Encapsulates the permission state and logic associated with the Captured
// Surface Control API. Objects of this class live on the IO thread.
class CONTENT_EXPORT CapturedSurfaceController {
 public:
  using CapturedSurfaceControlResult =
      ::blink::mojom::CapturedSurfaceControlResult;

  // Wheel deltas are clamped to this arbitrary,  reasonable value.
  // We clamp rather than report an error because "reasonable" is not
  // well-defined as of the time being, let alone in an interoperable way.
  inline static constexpr int32_t kMaxWheelDeltaMagnitude = 1000000;

  static std::unique_ptr<CapturedSurfaceController> CreateForTesting(
      GlobalRenderFrameHostId capturer_rfh_id,
      WebContentsMediaCaptureId captured_wc_id,
      std::unique_ptr<CapturedSurfaceControlPermissionManager>
          permission_manager,
      base::RepeatingCallback<void(int)> on_zoom_level_change_callback,
      base::RepeatingCallback<void(base::WeakPtr<WebContents>)>
          wc_resolution_callback);

  CapturedSurfaceController(
      GlobalRenderFrameHostId capturer_rfh_id,
      WebContentsMediaCaptureId captured_wc_id,
      base::RepeatingCallback<void(int)> on_zoom_level_change_callback);

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

  // Set the zoom level of the captured tab.
  virtual void SetZoomLevel(
      int zoom_level,
      base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback);

  virtual void RequestPermission(
      base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback);

  struct CapturedSurfaceInfo final {
    CapturedSurfaceInfo(
        base::WeakPtr<WebContents> captured_wc,
        std::unique_ptr<base::CallbackListSubscription,
                        BrowserThread::DeleteOnUIThread> subscription,
        int subscription_version,
        int initial_zoom_level);
    CapturedSurfaceInfo(CapturedSurfaceInfo&& other);
    CapturedSurfaceInfo& operator=(CapturedSurfaceInfo&& other);
    ~CapturedSurfaceInfo();

    base::WeakPtr<WebContents> captured_wc;
    std::unique_ptr<base::CallbackListSubscription,
                    BrowserThread::DeleteOnUIThread>
        subscription;
    int subscription_version;
    int initial_zoom_level;
  };

 private:
  using PermissionResult =
      CapturedSurfaceControlPermissionManager::PermissionResult;

  void OnZoomLevelChange(int subscription_version, int zoom_level);

  CapturedSurfaceController(
      GlobalRenderFrameHostId capturer_rfh_id,
      WebContentsMediaCaptureId captured_wc_id,
      std::unique_ptr<CapturedSurfaceControlPermissionManager>
          permission_manager,
      base::RepeatingCallback<void(int)> on_zoom_level_change_callback,
      base::RepeatingCallback<void(base::WeakPtr<WebContents>)>
          wc_resolution_callback);

  // Manage the resolution of WebContents-IDs into base::WeakPtr<WebContents>.
  void ResolveCapturedSurface(WebContentsMediaCaptureId captured_wc_id);
  void OnCapturedSurfaceResolved(
      std::optional<CapturedSurfaceInfo> captured_surface);

  const GlobalRenderFrameHostId capturer_rfh_id_;

  // References the captured tab through its WebContents.
  //
  // Set to nullopt when:
  // * The captured surface is not a tab.
  // * Right after construction, before the ID is first resolved (on the
  //   UI thread) to a valid base::WeakPtr<WebContents>.
  // * Whenever the captured tab changes, and UpdateCaptureTarget() is
  //   called. This triggers a new resolution, and in the intervening time,
  //   this will be set back to nullptr.
  //
  // Set to a concrete value otherwise.
  // However, this concrete value can be nullptr, (1) as with any WeakPtr,
  // or (2) if the ID failed to resolve to a valid WebContents.
  //
  // Note that `this` lives on the IO thread, and it is not possible to
  // check the value of the underlying WebContents* here, or even compare
  // it to nullptr.
  //
  // In the unlikely-yet-possible case that SendWheel() or SetZoomLevel()
  // are called while the task to resolve is pending, those calls will
  // fail gracefully. Subsequent calls are valid and can succeed.
  // TODO(crbug.com/41493349): Add UMA to measure how often this happens
  // and determine whether it's worth the effort to fix.
  std::optional<base::WeakPtr<WebContents>> captured_wc_;

  // Counts the pending resolutions, so that `captured_wc_` would only
  // be set to a concrete values when the last one resolves.
  int pending_wc_resolutions_ = 0;

  std::unique_ptr<CapturedSurfaceControlPermissionManager> permission_manager_;

  // Callback to be invoked whenever an ID's resolution to a
  // base::WeakPtr<WebContents> completes.
  const base::RepeatingCallback<void(base::WeakPtr<WebContents>)>
      wc_resolution_callback_;

  // `zoom_level_subscription_` controls the lifetime of a subscriptions to
  // zoom-level updates for a captured tab. If the capture is switched over to a
  // new tab, the subscription is re-initialized and `subscription_version_` is
  // incremented so that callback invocations from the previous subscriptions
  // may be ignored.
  std::unique_ptr<base::CallbackListSubscription,
                  BrowserThread::DeleteOnUIThread>
      zoom_level_subscription_;
  int subscription_version_ = 0;

  std::optional<int> current_zoom_level_;
  const base::RepeatingCallback<void(int)> on_zoom_level_change_callback_;

  base::WeakPtrFactory<CapturedSurfaceController> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURED_SURFACE_CONTROLLER_H_
