// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
struct MediaStreamRequest;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

class GURL;

namespace extensions {

// A delegate class of WebViewPermissionHelper to request permissions that are
// not a part of extensions.
class WebViewPermissionHelperDelegate {
 public:
  explicit WebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper);

  WebViewPermissionHelperDelegate(const WebViewPermissionHelperDelegate&) =
      delete;
  WebViewPermissionHelperDelegate& operator=(
      const WebViewPermissionHelperDelegate&) = delete;

  virtual ~WebViewPermissionHelperDelegate();

  virtual void CanDownload(const GURL& url,
                           const std::string& request_method,
                           base::OnceCallback<void(bool)> callback) {}

  virtual void RequestMediaAccessPermissionForControlledFrame(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) {}

  virtual bool CheckMediaAccessPermissionForControlledFrame(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type);

  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      base::OnceCallback<void(bool)> callback) {}

  // Requests Geolocation Permission from the embedder.
  virtual void RequestGeolocationPermission(
      const GURL& requesting_frame_url,
      bool user_gesture,
      base::OnceCallback<void(bool)> callback) {}

  virtual void RequestHidPermission(const GURL& requesting_frame_url,
                                    base::OnceCallback<void(bool)> callback) {}

  virtual void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      base::OnceCallback<void(bool)> callback) {}

  virtual void RequestFullscreenPermission(
      const url::Origin& requesting_origin,
      WebViewPermissionHelper::PermissionResponseCallback callback) {}

  // Called when file system access is requested by the guest content using the
  // asynchronous HTML5 file system API. The request is plumbed through the
  // <webview> permission request API. The request will be:
  // - Allowed if the embedder explicitly allowed it.
  // - Denied if the embedder explicitly denied.
  // - Determined by the guest's content settings if the embedder does not
  // perform an explicit action.
  // If access was blocked due to the page's content settings,
  // |blocked_by_policy| should be true, and this function should invoke
  // OnContentBlocked.
  virtual void FileSystemAccessedAsync(int render_process_id,
                                       int render_frame_id,
                                       int request_id,
                                       const GURL& url,
                                       bool blocked_by_policy) {}

  WebViewPermissionHelper* web_view_permission_helper() const {
    return web_view_permission_helper_;
  }

 private:
  const raw_ptr<WebViewPermissionHelper> web_view_permission_helper_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
