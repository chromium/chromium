// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_controller_delegate.h"

#include <memory>

#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

PermissionControllerDelegate::~PermissionControllerDelegate() {
  subscriptions_ = nullptr;
}

bool PermissionControllerDelegate::IsPermissionOverridable(
    blink::PermissionType permission,
    const std::optional<url::Origin>& origin) {
  return true;
}

PermissionResult
PermissionControllerDelegate::GetPermissionResultForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  return PermissionResult(PermissionStatus::DENIED,
                          PermissionStatusSource::UNSPECIFIED);
}

std::optional<gfx::Rect>
PermissionControllerDelegate::GetExclusionAreaBoundsInScreen(
    content::WebContents* web_contents) const {
  return std::nullopt;
}

void PermissionControllerDelegate::SetSubscriptions(
    content::PermissionController::SubscriptionsMap* subscriptions) {
  subscriptions_ = subscriptions;
}

content::PermissionController::SubscriptionsMap*
PermissionControllerDelegate::subscriptions() {
  return subscriptions_;
}

}  // namespace content
