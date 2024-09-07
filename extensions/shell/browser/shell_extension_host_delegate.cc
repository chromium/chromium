// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_host_delegate.h"

#include "base/notreached.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/media_capture_util.h"
#include "extensions/common/extension_id.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"

namespace extensions {

ShellExtensionHostDelegate::ShellExtensionHostDelegate() = default;
ShellExtensionHostDelegate::~ShellExtensionHostDelegate() = default;

void ShellExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

void ShellExtensionHostDelegate::OnMainFrameCreatedForBackgroundPage(
    ExtensionHost* host) {}

content::JavaScriptDialogManager*
ShellExtensionHostDelegate::GetJavaScriptDialogManager() {
  // TODO(jamescook): Create a JavaScriptDialogManager or reuse the one from
  // content_shell.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void ShellExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const ExtensionId& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  // TODO(jamescook): Should app_shell support opening popup windows?
  NOTREACHED_IN_MIGRATION();
}

void ShellExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const Extension* extension) {
  // Allow access to the microphone and/or camera.
  media_capture_util::GrantMediaStreamRequest(web_contents, request,
                                              std::move(callback), extension);
}

bool ShellExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  media_capture_util::VerifyMediaAccessPermission(type, extension);
  return true;
}

content::PictureInPictureResult
ShellExtensionHostDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  NOTREACHED_IN_MIGRATION();
  return content::PictureInPictureResult::kNotSupported;
}

void ShellExtensionHostDelegate::ExitPictureInPicture() {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace extensions
