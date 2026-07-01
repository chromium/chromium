// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_controller_delegate.h"

#include <memory>

#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

PermissionControllerDelegate::~PermissionControllerDelegate() {
  subscriptions_ = nullptr;
}

content::PermissionController::SubscriptionId
PermissionControllerDelegate::SubscribeToContentSettingsTypeChange(
    ContentSettingsType content_settings_type,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    base::RepeatingCallback<void(const PermissionSetting&)> callback) {
  return content::PermissionController::SubscriptionId();
}

void PermissionControllerDelegate::UnsubscribeFromContentSettingsTypeChange(
    content::PermissionController::SubscriptionId subscription_id) {}

bool PermissionControllerDelegate::IsPermissionOverridable(
    blink::PermissionType permission,
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin) {
  return true;
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
