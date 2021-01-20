// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include "base/macros.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace extensions {

// A delegate class of WebViewPermissionHelper to request permissions that are
// not a part of extensions.
class WebViewPermissionHelperDelegate : public content::WebContentsObserver {
 public:
  explicit WebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper);
  ~WebViewPermissionHelperDelegate() override;

  virtual void CanDownload(const GURL& url,
                           const std::string& request_method,
                           base::OnceCallback<void(bool)> callback) {}

  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      base::OnceCallback<void(bool)> callback) {}

  // Requests Geolocation Permission from the embedder.
  virtual void RequestGeolocationPermission(
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::OnceCallback<void(bool)> callback) {}

  virtual void CancelGeolocationPermissionRequest(int bridge_id) {}

  virtual void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      base::OnceCallback<void(bool)> callback) {}

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
  virtual void FileSystemAccessedAsync(
      int render_process_id,
      int render_frame_id,
      int request_id,
      const GURL& url,
      bool blocked_by_policy) {}

  WebViewPermissionHelper* web_view_permission_helper() const {
    return web_view_permission_helper_;
  }

 private:
  WebViewPermissionHelper* const web_view_permission_helper_;

  DISALLOW_COPY_AND_ASSIGN(WebViewPermissionHelperDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
