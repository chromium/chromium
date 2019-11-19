// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_web_contents_helper.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

namespace extensions {

AppWebContentsHelper::AppWebContentsHelper(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    content::WebContents* web_contents,
    AppDelegate* app_delegate)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      web_contents_(web_contents),
      app_delegate_(app_delegate) {
}

// static
bool AppWebContentsHelper::ShouldSuppressGestureEvent(
    const blink::WebGestureEvent& event) {
  // Disable "smart zoom" (double-tap with two fingers on Mac trackpad).
  if (event.GetType() == blink::WebInputEvent::kGestureDoubleTap)
    return true;

  // Disable pinch zooming in app windows.
  if (blink::WebInputEvent::IsPinchGestureEventType(event.GetType())) {
    // Only suppress pinch events that cause a scale change. We still
    // allow synthetic wheel events for touchpad pinch to go to the page.
    return !(event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
             event.NeedsWheelEvent());
  }

  return false;
}

content::WebContents* AppWebContentsHelper::OpenURLFromTab(
    const content::OpenURLParams& params) const {
  // Don't allow the current tab to be navigated. It would be nice to map all
  // anchor tags (even those without target="_blank") to new tabs, but right
  // now we can't distinguish between those and <meta> refreshes or window.href
  // navigations, which we don't want to allow.
  // TODO(mihaip): Can we check for user gestures instead?
  WindowOpenDisposition disposition = params.disposition;
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    web_contents_->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Can't open same-window link to \"%s\"; try target=\"_blank\".",
            params.url.spec().c_str()));
    return NULL;
  }

  // These dispositions aren't really navigations.
  if (disposition == WindowOpenDisposition::SAVE_TO_DISK ||
      disposition == WindowOpenDisposition::IGNORE_ACTION)
    return NULL;

  content::WebContents* contents =
      app_delegate_->OpenURLFromTab(browser_context_, web_contents_, params);
  if (!contents) {
    web_contents_->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Can't navigate to \"%s\"; apps do not support navigation.",
            params.url.spec().c_str()));
  }

  return contents;
}

void AppWebContentsHelper::RequestToLockMouse() const {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  bool has_permission = IsExtensionWithPermissionOrSuggestInConsole(
      APIPermission::kPointerLock, extension, web_contents_->GetMainFrame());

  web_contents_->GotResponseToLockMouseRequest(has_permission);
}

void AppWebContentsHelper::RequestMediaAccessPermission(
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  app_delegate_->RequestMediaAccessPermission(web_contents_, request,
                                              std::move(callback), extension);
}

bool AppWebContentsHelper::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  return app_delegate_->CheckMediaAccessPermission(
      render_frame_host, security_origin, type, extension);
}

const Extension* AppWebContentsHelper::GetExtension() const {
  return ExtensionRegistry::Get(browser_context_)
      ->enabled_extensions()
      .GetByID(extension_id_);
}

}  // namespace extensions
