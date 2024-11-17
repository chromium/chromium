# WebView

This directory contains key classes that are part of the
[WebView](https://developer.chrome.com/docs/apps/reference/webviewTag) feature
implementation, integrating //components/guest_view with the //extensions
system.

## Example - WebView permission request API behavior

The current [Webview permission request
API](https://developer.chrome.com/docs/apps/reference/webviewTag#event-permissionrequest)
supports a small set of [WebView
permissions](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_types.h).
Some of these permissions overlap with Web Permissions (e.g. media, geolocation,
HID, pointer lock). Some of these are WebView-specific permissions (new window,
javascript dialog, etc). In the browser process when a RenderFrameHost (RFH) is
created, the RFH is configured to support WebView's handling for particular
permission events (for plugins).

1.  [ChromeContentBrowserClient::RegisterAssociatedInterfaceBindersForRenderFrameHost()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/chrome_content_browser_client_receiver_bindings.cc;l=392;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=0;bpt=1)
adds a handler, BindPluginAuthHost, which binds receivers for the PluginAuthHost
interface. For auth messages from a RFH in a webview, the receiver is bound to
the webview's ChromeWebViewPermissionHelperDelegate. See
[Mojo &amp; Services](/docs/mojo_and_services.md) for details on message
passing.
1.  Now when an auth message is received, the message will be handled by
[ChromeWebViewPermissionHelperDelegate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h;l=22;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1).

When a WebView is created by the embedder, as part of WebView initialization in
the browser process the WebView system will create a handler for permission
events:

1.  [WebViewGuest::DidInitialize()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.cc;l=415;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733) - creates a [WebViewPermissionHelper](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.h;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;l=30) and stores it on [web_view_permission_helper_](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.h;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;l=360)
1.  In extensions (non-Chrome), when [WebViewPermissionHelper](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.h;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;l=30) starts it uses the ExtensionsAPIClient to create a [WebViewPermissionHelperDelegate](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h;l=30?q=web_view_permission_helper_delegate.h&ss=chromium%2Fchromium%2Fsrc). We'll focus just on the //chrome layer case:
    1.  [ChromeExtensionsAPIClient::CreateWebViewPermissionHelperDelegate()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/api/chrome_extensions_api_client.cc;l=326;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733) -- returns a [ChromeWebViewPermissionHelperDelegate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h;l=22;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1)

The WebView embedder adds an event handler for `permissionrequest` events:

1.  Ex: [pointerlock test](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/data/extensions/platform_apps/web_view/pointer_lock/main.js;l=16?q=addeventlistener%20permissionrequest%20webview&ss=chromium%2Fchromium%2Fsrc):
    1.  webview.addEventListener('permissionrequest', function(e) { ... });

When the WebView embedded content requests a permission:

1.  [Permissions::request()](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/permissions/permissions.cc;l=125-129;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733)
1.  (through multiple layers of mojom and then to the PermissionManager...)
1.  [PermissionManager::RequestPermissionsInternal()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_manager.cc;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1;l=239)
1.  [PermissionContextBase::RequestPermission()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.cc;l=240;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1)

At this point, some additional handling will occur that's permission-specific.
We'll skip over some of those steps but for one case -- geolocation -- we'll
provide some of that detail:

1.  [GeolocationPermissionContext::DecidePermission()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/contexts/geolocation_permission_context.cc;l=39-40;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1)
1.  [GeolocationPermissionContextDelegate::DecidePermission()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/geolocation/geolocation_permission_context_delegate.cc;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1;l=23)
1.  [GeolocationPermissionContextExtensions::DecidePermission()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/geolocation/geolocation_permission_context_extensions.cc;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=1;bpt=1;l=51)

For geolocation, the last DecidePermission() call is where the WebView code
intercepts the permission request. Stepping back to look at all of the
permission types, the browser relays the permission into permission-specific
code for each of those types similar to this. Here's a list of those intercepts,
including geolocation:

1.  The permission request is intercepted. The interception point is permission-specific.
    1.  [pointerlock](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.h;l=261;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a), [media](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.h;l=224;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a) both use a [WebContents](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents.h;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;l=145) pattern that allows a [WebContentsDelegate](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents_delegate.h;l=125?q=WebContentsDelegate&ss=chromium%2Fchromium%2Fsrc) to handle some auth requests. In these cases, [WebViewGuest](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.h;l=41?q=WebViewGuest&sq=&ss=chromium%2Fchromium%2Fsrc) acts as the [WebContentsDelegate](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents_delegate.h;l=125?q=WebContentsDelegate&ss=chromium%2Fchromium%2Fsrc).
        1.  Pointerlock is intercepted in [WebContentsImpl::RequestToLockPointer()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/web_contents/web_contents_impl.cc;l=4503;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=0;bpt=1).
        1.  Media is intercepted in [WebContentsImpl::RequestMediaAccessPermission()](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/web_contents/web_contents_impl.cc;l=5165;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733).
    1.  Geolocation - [GeolocationPermissionContextExtensions](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/geolocation/geolocation_permission_context_extensions.cc;l=51;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a) which inherits [GeolocationPermissionContextDelegate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/geolocation/geolocation_permission_context_delegate.h;l=20?q=GeolocationPermissionContextDelegate&sq=&ss=chromium%2Fchromium%2Fsrc). Intercepted in [GeolocationPermissionContextExtensions::DecidePermission()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/geolocation/geolocation_permission_context_extensions.cc;l=71-73;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733;bpv=0;bpt=1).
        1. Geolocation also has an override where it calls [GuestViewBase::OverridePermissionResult()](https://source.chromium.org/chromium/chromium/src/+/main:components/permissions/permission_context_base.cc;l=326;drc=2046c842c9a8e7abe63a74f26e05896c15daa258) to ensure that for whatever origin, the permission look up result will always be ASK which leads into GeolocationPermissionContext::DecidePermission(). This is Controlled Frame-specific.
    1.  HID checks whether a [RenderFrameHost](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/render_frame_host.h;l=138?q=RenderFrameHost%20file:.h$&ss=chromium%2Fchromium%2Fsrc) is in a WebView in [ChromeHidDelegate](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/hid/chrome_hid_delegate.cc;l=192;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a)
    1.  New Window is called from [WebViewGuest::CreateNewGuestWebViewWindow](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.cc;l=685;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a). Note this "newwindow" event is not from a permission request, but this is where we generate a permissionrequest event to capture it and send it to the embedder.
    1.  File System is called from [ChromeContentBrowserClient::AllowWorkerFileSystem](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/chrome_content_browser_client.cc;l=3114;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a) and [ContentSettingsManagerDelegate::AllowStorageAccess](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/content_settings/content_settings_manager_delegate.cc;l=82;drc=1149fe5f7bedbe8187bed8d6287a1ef19eac9b5a)
1.  An extension function fires a `permissionrequest` event.
    1.  The
    [WebViewPermissionHelper::RequestPermission()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;l=294;drc=90cac1911508d3d682a67c97aa62483eb712f69a)
    call will dispatch a [webview::kEventPermissionRequest
    event](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_constants.cc;drc=90cac1911508d3d682a67c97aa62483eb712f69a;l=28)
    to the WebView host.
1.  The event is routed to the WebView host's script. If the event is handled
and `request.allow()` is called, the embedder indicates that the permission
should be granted. But the grant is still subject to the embedder being trusted
to grant the permission (see below).
    1.  Embedder's call to allow() invokes:
        1.  [PermissionRequest.prototype.allow()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/renderer/resources/guest_view/web_view/web_view_action_requests.js;l=249?q=file:%5Eextensions%20allow%20permissionrequest&ss=chromium%2Fchromium%2Fsrc)
        1.  [WebViewInternalSetPermissionFunction::Run()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/guest_view/web_view/web_view_internal_api.cc;l=965;drc=90cac1911508d3d682a67c97aa62483eb712f69a)
        1.  [WebViewPermissionHelper::SetPermission()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;drc=90cac1911508d3d682a67c97aa62483eb712f69a;l=340)
    1. For media and geolocation, an additional permission call happens with the
    requester transformed from the embedded document to the embedder document,
    as if the embedder is requesting the permission itself
    ([media](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;l=259;drc=b4abe28c5335263623caa6de4c047ce0ba8bfe8b),
    [geolocation](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.cc;l=187;drc=cc5be7150eef183a1b9a6716d42a396ab7c59733)).
1. [SetPermission()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;drc=90cac1911508d3d682a67c97aa62483eb712f69a;l=340)
finds the associated permission request, [runs the associated
callback](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;l=353;drc=90cac1911508d3d682a67c97aa62483eb712f69a)
(passing the permission request result to the callback), then cleans up the
entry from the permission request list.
    1.  The associated callback will have been set for each of the different types of permission requests. Here are the callbacks:
        1.  Geolocation - [ChromeWebViewPermissionHelperDelegate:: OnGeolocationPermissionResponse()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.cc;l=175;drc=eca0d3645e88876fefaf2ed1f1e6164fbde8a1b5)
        1.  media - [WebViewPermissionHelper::OnMediaPermissionResponse()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_permission_helper.cc;l=221;drc=eca0d3645e88876fefaf2ed1f1e6164fbde8a1b5)
        1.  pointerlock - [ChromeWebViewPermissionHelperDelegate:: OnPointerLockPermissionResponse()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.cc;l=147;drc=eca0d3645e88876fefaf2ed1f1e6164fbde8a1b5)
        1.  HID - [ChromeWebViewPermissionHelperDelegate::OnHidPermissionResponse()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.cc;l=212;drc=eca0d3645e88876fefaf2ed1f1e6164fbde8a1b5)
1. Once the embedder allows a permission request, depending on the permission type,
the embedder's permission is also checked.

All other PermissionTypes, except for geolocation, audio and video, are
explicitly rejected as ["not
requestable"](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/guest_view/web_view/web_view_guest.cc;l=1145;drc=5c8e0b22fce77592e386624c5c25be6fad037958)
in WebView.
