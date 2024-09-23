// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/common/extension_features.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

using base::UserMetricsAction;
using content::BrowserPluginGuestDelegate;
using guest_view::GuestViewEvent;

namespace extensions {

namespace {
static std::string PermissionTypeToString(WebViewPermissionType type) {
  switch (type) {
    case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
      return webview::kPermissionTypeDownload;
    case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
      return webview::kPermissionTypeFileSystem;
    case WEB_VIEW_PERMISSION_TYPE_FULLSCREEN:
      return webview::kPermissionTypeFullscreen;
    case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
      return webview::kPermissionTypeGeolocation;
    case WEB_VIEW_PERMISSION_TYPE_HID:
      return webview::kPermissionTypeHid;
    case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
      return webview::kPermissionTypeDialog;
    case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
      return webview::kPermissionTypeLoadPlugin;
    case WEB_VIEW_PERMISSION_TYPE_MEDIA:
      return webview::kPermissionTypeMedia;
    case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
      return webview::kPermissionTypeNewWindow;
    case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
      return webview::kPermissionTypePointerLock;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// static
void RecordUserInitiatedUMA(
    const WebViewPermissionHelper::PermissionResponseInfo& info,
    bool allow) {
  if (allow) {
    // Note that |allow| == true means the embedder explicitly allowed the
    // request. For some requests they might still fail. An example of such
    // scenario would be: an embedder allows geolocation request but doesn't
    // have geolocation access on its own.
    switch (info.permission_type) {
      case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Download"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.FileSystem"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_FULLSCREEN:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Fullscreen"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.Geolocation"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_HID:
        base::RecordAction(UserMetricsAction("WebView.PermissionAllow.HID"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.JSDialog"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
        base::RecordAction(
            UserMetricsAction("WebView.Guest.PermissionAllow.PluginLoad"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_MEDIA:
        base::RecordAction(UserMetricsAction("WebView.PermissionAllow.Media"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
        base::RecordAction(
            UserMetricsAction("BrowserPlugin.PermissionAllow.NewWindow"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionAllow.PointerLock"));
        break;
      default:
        break;
    }
  } else {
    switch (info.permission_type) {
      case WEB_VIEW_PERMISSION_TYPE_DOWNLOAD:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Download"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_FILESYSTEM:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.FileSystem"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_FULLSCREEN:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Fullscreen"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_GEOLOCATION:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.Geolocation"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_HID:
        base::RecordAction(UserMetricsAction("WebView.PermissionDeny.HID"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.JSDialog"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN:
        base::RecordAction(
            UserMetricsAction("WebView.Guest.PermissionDeny.PluginLoad"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_MEDIA:
        base::RecordAction(UserMetricsAction("WebView.PermissionDeny.Media"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW:
        base::RecordAction(
            UserMetricsAction("BrowserPlugin.PermissionDeny.NewWindow"));
        break;
      case WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK:
        base::RecordAction(
            UserMetricsAction("WebView.PermissionDeny.PointerLock"));
        break;
      default:
        break;
    }
  }
}

} // namespace

WebViewPermissionHelper::WebViewPermissionHelper(WebViewGuest* web_view_guest)
    : next_permission_request_id_(guest_view::kInstanceIDNone),
      web_view_guest_(web_view_guest) {
  web_view_permission_helper_delegate_.reset(
      ExtensionsAPIClient::Get()->CreateWebViewPermissionHelperDelegate(this));
}

WebViewPermissionHelper::~WebViewPermissionHelper() {
}

// static
WebViewPermissionHelper* WebViewPermissionHelper::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  WebViewGuest* web_view_guest =
      WebViewGuest::FromRenderFrameHost(render_frame_host);
  return web_view_guest ? web_view_guest->web_view_permission_helper()
                        : nullptr;
}

// static
WebViewPermissionHelper* WebViewPermissionHelper::FromRenderFrameHostId(
    const content::GlobalRenderFrameHostId& render_frame_host_id) {
  WebViewGuest* web_view_guest =
      WebViewGuest::FromRenderFrameHostId(render_frame_host_id);
  return web_view_guest ? web_view_guest->web_view_permission_helper()
                        : nullptr;
}

void WebViewPermissionHelper::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, request.security_origin.spec());
  RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_MEDIA, std::move(request_info),
      base::BindOnce(&WebViewPermissionHelper::OnMediaPermissionResponse,
                     weak_factory_.GetWeakPtr(), request, std::move(callback)),
      /*allowed_by_default=*/false);
}

void WebViewPermissionHelper::RequestMediaAccessPermissionForControlledFrame(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  web_view_permission_helper_delegate_
      ->RequestMediaAccessPermissionForControlledFrame(source, request,
                                                       std::move(callback));
}

bool WebViewPermissionHelper::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  if (!web_view_guest()->attached() ||
      !web_view_guest()->embedder_web_contents()->GetDelegate()) {
    return false;
  }
  return web_view_guest()
      ->embedder_web_contents()
      ->GetDelegate()
      ->CheckMediaAccessPermission(web_view_guest()
                                       ->GetGuestMainFrame()
                                       ->GetParentOrOuterDocumentOrEmbedder(),
                                   security_origin, type);
}

bool WebViewPermissionHelper::CheckMediaAccessPermissionForControlledFrame(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return web_view_permission_helper_delegate_
      ->CheckMediaAccessPermissionForControlledFrame(render_frame_host,
                                                     security_origin, type);
}

void WebViewPermissionHelper::OnMediaPermissionResponse(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool allow,
    const std::string& user_input) {
  if (!allow) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  if (!web_view_guest()->attached() ||
      !web_view_guest()->embedder_web_contents()->GetDelegate()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::INVALID_STATE,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  web_view_guest()
      ->embedder_web_contents()
      ->GetDelegate()
      ->RequestMediaAccessPermission(web_view_guest()->embedder_web_contents(),
                                     request, std::move(callback));
}

void WebViewPermissionHelper::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  web_view_permission_helper_delegate_->CanDownload(url, request_method,
                                                    std::move(callback));
}

void WebViewPermissionHelper::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    base::OnceCallback<void(bool)> callback) {
  web_view_permission_helper_delegate_->RequestPointerLockPermission(
      user_gesture, last_unlocked_by_target, std::move(callback));
}

void WebViewPermissionHelper::RequestGeolocationPermission(
    const GURL& requesting_frame_url,
    bool user_gesture,
    base::OnceCallback<void(bool)> callback) {
  web_view_permission_helper_delegate_->RequestGeolocationPermission(
      requesting_frame_url, user_gesture, std::move(callback));
}

void WebViewPermissionHelper::RequestHidPermission(
    const GURL& requesting_frame_url,
    base::OnceCallback<void(bool)> callback) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kEnableWebHidInWebView)) {
    std::move(callback).Run(false);
    return;
  }

  web_view_permission_helper_delegate_->RequestHidPermission(
      requesting_frame_url, std::move(callback));
}

void WebViewPermissionHelper::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    base::OnceCallback<void(bool)> callback) {
  web_view_permission_helper_delegate_->RequestFileSystemPermission(
      url, allowed_by_default, std::move(callback));
}

void WebViewPermissionHelper::RequestFullscreenPermission(
    const url::Origin& requesting_origin,
    PermissionResponseCallback callback) {
  web_view_permission_helper_delegate_->RequestFullscreenPermission(
      requesting_origin, std::move(callback));
}

int WebViewPermissionHelper::RequestPermission(
    WebViewPermissionType permission_type,
    base::Value::Dict request_info,
    PermissionResponseCallback callback,
    bool allowed_by_default) {
  // If there are too many pending permission requests then reject this request.
  if (pending_permission_requests_.size() >=
      webview::kMaxOutstandingPermissionRequests) {
    // Let the stack unwind before we deny the permission request so that
    // objects held by the permission request are not destroyed immediately
    // after creation. This is to allow those same objects to be accessed again
    // in the same scope without fear of use after freeing.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), allowed_by_default, std::string()));
    return webview::kInvalidPermissionRequestID;
  }

  int request_id = next_permission_request_id_++;
  pending_permission_requests_[request_id] = PermissionResponseInfo(
      std::move(callback), permission_type, allowed_by_default);
  base::Value::Dict args;
  args.Set(webview::kRequestInfo, std::move(request_info));
  args.Set(webview::kRequestId, request_id);
  switch (permission_type) {
    case WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW: {
      web_view_guest_->DispatchEventToView(std::make_unique<GuestViewEvent>(
          webview::kEventNewWindow, std::move(args)));
      break;
    }
    case WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG: {
      web_view_guest_->DispatchEventToView(std::make_unique<GuestViewEvent>(
          webview::kEventDialog, std::move(args)));
      break;
    }
    default: {
      args.Set(webview::kPermission, PermissionTypeToString(permission_type));
      web_view_guest_->DispatchEventToView(std::make_unique<GuestViewEvent>(
          webview::kEventPermissionRequest, std::move(args)));
      break;
    }
  }
  return request_id;
}

WebViewPermissionHelper::SetPermissionResult
WebViewPermissionHelper::SetPermission(
    int request_id,
    PermissionResponseAction action,
    const std::string& user_input) {
  auto request_itr = pending_permission_requests_.find(request_id);

  if (request_itr == pending_permission_requests_.end())
    return SET_PERMISSION_INVALID;

  PermissionResponseInfo& info = request_itr->second;
  bool allow = (action == ALLOW) ||
      ((action == DEFAULT) && info.allowed_by_default);

  std::move(info.callback).Run(allow, user_input);

  // Only record user initiated (i.e. non-default) actions.
  if (action != DEFAULT)
    RecordUserInitiatedUMA(info, allow);

  pending_permission_requests_.erase(request_itr);

  return allow ? SET_PERMISSION_ALLOWED : SET_PERMISSION_DENIED;
}

void WebViewPermissionHelper::CancelPendingPermissionRequest(int request_id) {
  auto request_itr = pending_permission_requests_.find(request_id);

  if (request_itr == pending_permission_requests_.end())
    return;

  pending_permission_requests_.erase(request_itr);
}

WebViewPermissionHelper::PermissionResponseInfo::PermissionResponseInfo()
    : permission_type(WEB_VIEW_PERMISSION_TYPE_UNKNOWN),
      allowed_by_default(false) {
}

WebViewPermissionHelper::PermissionResponseInfo::PermissionResponseInfo(
    PermissionResponseCallback callback,
    WebViewPermissionType permission_type,
    bool allowed_by_default)
    : callback(std::move(callback)),
      permission_type(permission_type),
      allowed_by_default(allowed_by_default) {}

WebViewPermissionHelper::PermissionResponseInfo&
WebViewPermissionHelper::PermissionResponseInfo::operator=(
    WebViewPermissionHelper::PermissionResponseInfo&& other) = default;

WebViewPermissionHelper::PermissionResponseInfo::~PermissionResponseInfo() =
    default;

}  // namespace extensions
