// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/permissions_tab_helper.h"

#import "base/timer/timer.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/web/permissions_infobar_delegate.h"
#import "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const float kTimeoutInMillisecond = 250;
}  // namespace

PermissionsTabHelper::PermissionsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  if (@available(iOS 15.0, *)) {
    if (web::features::IsMediaPermissionsControlEnabled()) {
      DCHECK(web_state);
      permissions_to_state_ =
          [web_state->GetStatesForAllPermissions() mutableCopy];
      web_state_->AddObserver(this);
    }
  }
}

PermissionsTabHelper::~PermissionsTabHelper() {}

void PermissionsTabHelper::PermissionStateChanged(web::WebState* web_state,
                                                  web::Permission permission) {
  DCHECK_EQ(web_state_, web_state);
  web::PermissionState new_state =
      web_state_->GetStateForPermission(permission);
  // Removes infobar if no permission is accessible.
  if (new_state == web::PermissionStateNotAccessible) {
    permissions_to_state_[@(permission)] = @(new_state);
    BOOL shouldRemoveInfobar = YES;
    for (NSNumber* permission in permissions_to_state_) {
      if (permissions_to_state_[permission].unsignedIntegerValue >
          web::PermissionStateNotAccessible) {
        shouldRemoveInfobar = NO;
        break;
      }
    }
    if (shouldRemoveInfobar && infobar_ != nullptr) {
      infobars::InfoBarManager* infobar_manager =
          InfoBarManagerImpl::FromWebState(web_state_);
      infobar_manager->RemoveInfoBar(infobar_);
    }
    return;
  }
  // Adds/replaces infobar if previous state was "NotAccessible".
  if (permissions_to_state_[@(permission)].unsignedIntegerValue ==
      web::PermissionStateNotAccessible) {
    // Delays infobar creation in case that multiple permissions are enabled
    // simultaneously to show one infobar with a message that says so, instead
    // of showing multiple infobar banners back to back.
    if (!timer_.IsRunning()) {
      recently_accessible_permissions_ = [NSMutableArray array];
      timer_.Start(FROM_HERE, base::Milliseconds(kTimeoutInMillisecond), this,
                   &PermissionsTabHelper::ShowInfoBar);
    }
    [recently_accessible_permissions_ addObject:@(permission)];
  }
  permissions_to_state_[@(permission)] = @(new_state);
  if (infobar_ != nullptr) {
    UpdateIsInfoBarAccepted();
  }
}

void PermissionsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  if (web::features::IsMediaPermissionsControlEnabled()) {
    web_state_->RemoveObserver(this);
  }
  web_state_ = nullptr;
}

void PermissionsTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (infobar == infobar_) {
    infobar_ = nullptr;
  }
}

void PermissionsTabHelper::ShowInfoBar() {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);

  std::unique_ptr<PermissionsInfoBarDelegate> delegate(
      std::make_unique<PermissionsInfoBarDelegate>(
          recently_accessible_permissions_, web_state_));

  BOOL first_activation = infobar_ == nullptr;
  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypePermissions, std::move(delegate));
  infobar_ = infobar_manager->AddInfoBar(std::move(infobar), true);
  if (first_activation) {
    UpdateIsInfoBarAccepted();
  }
}

void PermissionsTabHelper::UpdateIsInfoBarAccepted() {
  if (infobar_ == nullptr) {
    return;
  }
  BOOL accepted = NO;
  for (NSNumber* permission in permissions_to_state_) {
    if (permissions_to_state_[permission].unsignedIntegerValue ==
        web::PermissionStateAllowed) {
      accepted = YES;
      break;
    }
  }
  static_cast<InfoBarIOS*>(infobar_)->set_accepted(accepted);
}

WEB_STATE_USER_DATA_KEY_IMPL(PermissionsTabHelper)
