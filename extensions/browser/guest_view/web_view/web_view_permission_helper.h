// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace extensions {

class WebViewGuest;
class WebViewPermissionHelperDelegate;

// WebViewPermissionHelper manages <webview> permission requests. This helper
// class is owned by WebViewGuest. Its purpose is to request permission for
// various operations from the <webview> embedder, and reply back via callbacks
// to the callers on a response from the embedder.
class WebViewPermissionHelper
      : public content::WebContentsObserver {
 public:
  explicit WebViewPermissionHelper(WebViewGuest* guest);
  ~WebViewPermissionHelper() override;
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
                        const base::DictionaryValue& request_info,
                        PermissionResponseCallback callback,
                        bool allowed_by_default);

  static WebViewPermissionHelper* FromWebContents(
      content::WebContents* web_contents);
  static WebViewPermissionHelper* FromFrameID(int render_process_id,
                                              int render_frame_id);
  void RequestMediaAccessPermission(content::WebContents* source,
                                    const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type);
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback);
  void RequestPointerLockPermission(bool user_gesture,
                                    bool last_unlocked_by_target,
                                    base::OnceCallback<void(bool)> callback);

  // Requests Geolocation Permission from the embedder.
  void RequestGeolocationPermission(int bridge_id,
                                    const GURL& requesting_frame,
                                    bool user_gesture,
                                    base::OnceCallback<void(bool)> callback);
  void CancelGeolocationPermissionRequest(int bridge_id);

  void RequestFileSystemPermission(const GURL& url,
                                   bool allowed_by_default,
                                   base::OnceCallback<void(bool)> callback);

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

  void set_default_media_access_permission(bool allow_media_access) {
    default_media_access_permission_ = allow_media_access;
  }

 private:
  void OnMediaPermissionResponse(const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 bool allow,
                                 const std::string& user_input);

#if BUILDFLAG(ENABLE_PLUGINS)
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  // A counter to generate a unique request id for a permission request.
  // We only need the ids to be unique for a given WebViewGuest.
  int next_permission_request_id_;

  RequestMap pending_permission_requests_;

  std::unique_ptr<WebViewPermissionHelperDelegate>
      web_view_permission_helper_delegate_;

  WebViewGuest* const web_view_guest_;

  bool default_media_access_permission_;

  base::WeakPtrFactory<WebViewPermissionHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebViewPermissionHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_H_
