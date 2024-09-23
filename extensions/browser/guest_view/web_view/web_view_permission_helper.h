// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace url {
class Origin;
}  // namespace url

namespace extensions {

class WebViewGuest;
class WebViewPermissionHelperDelegate;

// WebViewPermissionHelper manages <webview> permission requests. This helper
// class is owned by WebViewGuest. Its purpose is to request permission for
// various operations from the <webview> embedder, and reply back via callbacks
// to the callers on a response from the embedder.
class WebViewPermissionHelper {
 public:
  explicit WebViewPermissionHelper(WebViewGuest* guest);

  WebViewPermissionHelper(const WebViewPermissionHelper&) = delete;
  WebViewPermissionHelper& operator=(const WebViewPermissionHelper&) = delete;

  ~WebViewPermissionHelper();
  using PermissionResponseCallback =
      base::OnceCallback<void(bool /* allow */,
                              const std::string& /* user_input */)>;

  // A map to store the callback for a request keyed by the request's id.
  struct PermissionResponseInfo {
    PermissionResponseCallback callback;
    WebViewPermissionType permission_type;
    bool allowed_by_default;
    PermissionResponseInfo();
    PermissionResponseInfo(PermissionResponseCallback callback,
                           WebViewPermissionType permission_type,
                           bool allowed_by_default);
    PermissionResponseInfo& operator=(PermissionResponseInfo&& other);
    ~PermissionResponseInfo();
  };

  using RequestMap = std::map<int, PermissionResponseInfo>;

  int RequestPermission(WebViewPermissionType permission_type,
                        base::Value::Dict request_info,
                        PermissionResponseCallback callback,
                        bool allowed_by_default);

  static WebViewPermissionHelper* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);
  static WebViewPermissionHelper* FromRenderFrameHostId(
      const content::GlobalRenderFrameHostId& render_frame_host_id);

  void RequestMediaAccessPermission(content::WebContents* source,
                                    const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);

  void RequestMediaAccessPermissionForControlledFrame(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback);

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type);

  bool CheckMediaAccessPermissionForControlledFrame(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type);

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback);
  void RequestPointerLockPermission(bool user_gesture,
                                    bool last_unlocked_by_target,
                                    base::OnceCallback<void(bool)> callback);

  // Requests Geolocation Permission from the embedder.
  void RequestGeolocationPermission(const GURL& requesting_frame,
                                    bool user_gesture,
                                    base::OnceCallback<void(bool)> callback);
  // Requests permission from the embedder to request access to Human
  // Interface Devices.
  void RequestHidPermission(const GURL& requesting_frame,
                            base::OnceCallback<void(bool)> callback);

  void RequestFileSystemPermission(const GURL& url,
                                   bool allowed_by_default,
                                   base::OnceCallback<void(bool)> callback);

  void RequestFullscreenPermission(const url::Origin& requesting_origin,
                                   PermissionResponseCallback callback);

  enum PermissionResponseAction { DENY, ALLOW, DEFAULT };

  enum SetPermissionResult {
    SET_PERMISSION_INVALID,
    SET_PERMISSION_ALLOWED,
    SET_PERMISSION_DENIED
  };

  // Responds to the permission request |request_id| with |action| and
  // |user_input|. Returns whether there was a pending request for the provided
  // |request_id|.
  SetPermissionResult SetPermission(int request_id,
                                    PermissionResponseAction action,
                                    const std::string& user_input);

  void CancelPendingPermissionRequest(int request_id);

  WebViewGuest* web_view_guest() { return web_view_guest_; }

  WebViewPermissionHelperDelegate* delegate() {
    return web_view_permission_helper_delegate_.get();
  }

 private:
  void OnMediaPermissionResponse(const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 bool allow,
                                 const std::string& user_input);

  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given WebViewGuest.
  int next_permission_request_id_;

  RequestMap pending_permission_requests_;

  std::unique_ptr<WebViewPermissionHelperDelegate>
      web_view_permission_helper_delegate_;

  const raw_ptr<WebViewGuest> web_view_guest_;

  base::WeakPtrFactory<WebViewPermissionHelper> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
