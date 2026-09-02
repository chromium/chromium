// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"

#import "base/task/sequenced_task_runner.h"
#import "base/timer/timer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/permissions_dialog_overlay.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

std::optional<ContentSettingsType> ContentSettingsTypeForPermission(
    web::Permission permission) {
  switch (permission) {
    // TODO(crbug.com/552561353): Add support for geolocation permissions.
    case web::PermissionCamera:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
    case web::PermissionMicrophone:
      return ContentSettingsType::MEDIASTREAM_MIC;
  }
}

namespace {

constexpr base::TimeDelta kTimeout = base::Milliseconds(250);

// Commits the user's sticky permission decision to HostContentSettingsMap.
void CommitPermissionDecisionToHostContentSettingsMap(
    web::WebState* web_state,
    const GURL& url,
    NSArray<NSNumber*>* permissions,
    PermissionDialogDecision decision) {
  if (!web_state || !url.is_valid()) {
    return;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  if (!profile) {
    return;
  }
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);
  if (!settings_map) {
    return;
  }

  ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
  switch (decision) {
    case PermissionDialogDecision::kAlwaysAllow:
      content_setting = CONTENT_SETTING_ALLOW;
      break;
    case PermissionDialogDecision::kDontAllow:
      content_setting = CONTENT_SETTING_BLOCK;
      break;
    case PermissionDialogDecision::kAllowThisTime:
      // Temporary access only.
      return;
  }

  for (NSNumber* permission_number in permissions) {
    web::Permission permission =
        static_cast<web::Permission>(permission_number.unsignedIntegerValue);
    std::optional<ContentSettingsType> type =
        ContentSettingsTypeForPermission(permission);
    if (!type) {
      continue;
    }

    settings_map->SetContentSettingDefaultScope(url, url, *type,
                                                content_setting);
  }
}

// Completion callback for permissions alert overlay.
void HandlePermissionDialogResponse(
    base::WeakPtr<web::WebState> weak_web_state,
    const GURL& requesting_url,
    NSArray<NSNumber*>* permissions,
    web::WebStatePermissionDecisionHandler handler,
    OverlayResponse* response) {
  PermissionsDialogResponse* dialog_response =
      response ? response->GetInfo<PermissionsDialogResponse>() : nullptr;
  web::PermissionDecision decision = web::PermissionDecisionDeny;
  if (dialog_response) {
    if (IsDomainLevelSitePermissionsEnabled() && weak_web_state) {
      CommitPermissionDecisionToHostContentSettingsMap(
          weak_web_state.get(), requesting_url, permissions,
          dialog_response->decision());
    }
    decision = dialog_response->capture_allow() ? web::PermissionDecisionGrant
                                                : web::PermissionDecisionDeny;
  }

  // Post the decision handler asynchronously to prevent synchronous re-entrancy
  // and stack overflow if WebKit immediately initiates another permission
  // request upon decision completion (e.g., when permissions are repeatedly
  // requested in a recursion loop).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](web::WebStatePermissionDecisionHandler callback,
                        web::PermissionDecision permission_decision) {
                       callback(permission_decision);
                     },
                     handler, decision));
}

}  // namespace

PermissionsTabHelper::PermissionsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state);
  permissions_to_state_ = [web_state->GetStatesForAllPermissions() mutableCopy];
  banner_queue_ = OverlayRequestQueue::FromWebState(
      web_state, OverlayModality::kInfobarBanner);
  inserter_ = InfobarOverlayRequestInserter::FromWebState(web_state);
  web_state_->AddObserver(this);
}

PermissionsTabHelper::~PermissionsTabHelper() {}

void PermissionsTabHelper::
    PresentPermissionsDecisionDialogWithCompletionHandler(
        NSArray<NSNumber*>* permissions,
        web::WebStatePermissionDecisionHandler handler) {
  GURL requesting_url = web_state_->GetLastCommittedURL();
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<PermissionsDialogRequest>(requesting_url,
                                                                 permissions);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&HandlePermissionDialogResponse, web_state_->GetWeakPtr(),
                     requesting_url, permissions, handler));
  OverlayRequestQueue::FromWebState(web_state_,
                                    OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

void PermissionsTabHelper::PermissionStateChanged(web::WebState* web_state,
                                                  web::Permission permission) {
  DCHECK_EQ(web_state_, web_state);
  web::PermissionState new_state =
      web_state_->GetStateForPermission(permission);
  // Removes infobar if no permission is accessible.
  if (new_state == web::PermissionStateNotAccessible) {
    permissions_to_state_[@(permission)] = @(new_state);
    BOOL shouldRemoveInfobar = YES;
    for (NSNumber* permission_number in permissions_to_state_) {
      if (permissions_to_state_[permission_number].unsignedIntegerValue >
          web::PermissionStateNotAccessible) {
        shouldRemoveInfobar = NO;
        break;
      }
    }
    if (infobar_ != nullptr) {
      if (shouldRemoveInfobar) {
        infobars::InfoBarManager* infobar_manager =
            InfoBarManagerImpl::FromWebState(web_state_);
        infobar_manager->RemoveInfoBar(infobar_);
      } else {
        UpdateIsInfoBarAccepted();
      }
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
      timer_.Start(FROM_HERE, kTimeout, this,
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
  DCHECK(banner_queue_);
  web_state_->RemoveObserver(this);

  web_state_ = nullptr;
  banner_queue_ = nullptr;
  inserter_ = nullptr;
}

void PermissionsTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (infobar == infobar_) {
    infobar_manager_scoped_observation_.Reset();
    infobar_ = nullptr;
  }
}

void PermissionsTabHelper::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  DCHECK(infobar_manager_scoped_observation_.IsObservingSource(manager));
  infobar_manager_scoped_observation_.Reset();
}

void PermissionsTabHelper::ShowInfoBar() {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  if (!infobar_manager_scoped_observation_.IsObservingSource(infobar_manager)) {
    infobar_manager_scoped_observation_.Observe(infobar_manager);
  }

  std::unique_ptr<PermissionsInfobarDelegate> delegate(
      std::make_unique<PermissionsInfobarDelegate>(
          recently_accessible_permissions_, web_state_));

  BOOL first_activation = infobar_ == nullptr;
  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypePermissions, std::move(delegate));
  infobar_ = infobar_manager->AddInfoBar(std::move(infobar), true);

  UpdateIsInfoBarAccepted();
  // Infobar replacement does not display the banner a second time, hence here
  // we use overlay inserter to manually show it again.
  if (!first_activation) {
    size_t index = 0;
    bool request_found = GetInfobarOverlayRequestIndex(
        banner_queue_, static_cast<InfoBarIOS*>(infobar_), &index);
    if (request_found) {  // The new banner is already shown.
      return;
    }
    InsertParams params(static_cast<InfoBarIOS*>(infobar_));
    params.overlay_type = InfobarOverlayType::kBanner;
    params.insertion_index = banner_queue_->size();
    params.source = InfobarOverlayInsertionSource::kInfoBarDelegate;
    inserter_->InsertOverlayRequest(params);
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
