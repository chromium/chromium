// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/coordinator/page_info_permissions_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "ios/chrome/browser/permissions/model/permissions_metrics.h"
#import "ios/chrome/browser/permissions/model/permissions_tab_helper.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_metrics_util.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "url/gurl.h"

namespace {

// Returns the ContentSetting corresponding to `setting`.
ContentSetting ContentSettingForSitePermissionSetting(
    SitePermissionSetting setting) {
  switch (setting) {
    case SitePermissionSetting::kAlwaysAllow:
      return CONTENT_SETTING_ALLOW;
    case SitePermissionSetting::kNeverAllow:
      return CONTENT_SETTING_BLOCK;
    case SitePermissionSetting::kAllowOnce:
      return CONTENT_SETTING_DEFAULT;
  }
}

// Returns the SitePermissionSetting corresponding to `content_setting`.
SitePermissionSetting SitePermissionSettingForContentSetting(
    ContentSetting content_setting) {
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      return SitePermissionSetting::kAlwaysAllow;
    case CONTENT_SETTING_BLOCK:
      return SitePermissionSetting::kNeverAllow;
    default:
      return SitePermissionSetting::kAllowOnce;
  }
}

// Returns the `IOSPermissionSetting` corresponding to `setting`.
IOSPermissionSetting IOSPermissionSettingForSitePermissionSetting(
    SitePermissionSetting setting) {
  switch (setting) {
    case SitePermissionSetting::kAllowOnce:
      return IOSPermissionSetting::kAllowOnce;
    case SitePermissionSetting::kAlwaysAllow:
      return IOSPermissionSetting::kAlwaysAllow;
    case SitePermissionSetting::kNeverAllow:
      return IOSPermissionSetting::kNeverAllow;
  }
}

}  // namespace

@interface PageInfoPermissionsMediator () <CRWWebStateObserver>
@end

@implementation PageInfoPermissionsMediator {
  raw_ptr<web::WebState> _webState;
  raw_ptr<HostContentSettingsMap> _hostContentSettingsMap;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

- (instancetype)initWithWebState:(web::WebState*)webState
          hostContentSettingsMap:
              (HostContentSettingsMap*)hostContentSettingsMap {
  self = [super init];
  if (self) {
    _webState = webState;
    _hostContentSettingsMap = hostContentSettingsMap;
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_observer.get());
  }
  return self;
}

- (void)setConsumer:(id<PermissionsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [self dispatchInitialPermissionsInfo];
}

- (void)disconnect {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
  _hostContentSettingsMap = nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didChangeStateForPermission:(web::Permission)permission {
  PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
  permissionsDescription.permission = permission;
  permissionsDescription.state = _webState->GetStateForPermission(permission);
  if (IsDomainLevelSitePermissionsEnabled()) {
    permissionsDescription.setting = [self permissionSettingFor:permission];
  }
  [self.consumer permissionStateChanged:permissionsDescription];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
}

#pragma mark - PermissionsDelegate

- (void)updatePermissionInfo:(PermissionInfo*)permissionInfo {
  if (!_webState) {
    return;
  }

  if (IsDomainLevelSitePermissionsEnabled()) {
    [self persistSetting:permissionInfo.setting
           forPermission:permissionInfo.permission];
    RecordPermissionSettingChanged(
        IOSPermissionSettingChangeSurface::kPageInfo, permissionInfo.permission,
        IOSPermissionSettingForSitePermissionSetting(permissionInfo.setting));
  } else {
    RecordPermissionToogled();
  }

  _webState->SetStateForPermission(permissionInfo.state,
                                   permissionInfo.permission);
  RecordPermissionEventFromOrigin(
      permissionInfo, PermissionEventOrigin::PermissionEventOriginPageInfo);
}

#pragma mark - Private

// Helper that creates and dispatches initial permissions information to the
// consumer.
- (void)dispatchInitialPermissionsInfo {
  NSMutableArray<PermissionInfo*>* permissionsInfo =
      [[NSMutableArray alloc] init];

  NSDictionary<NSNumber*, NSNumber*>* statesForAllPermissions =
      _webState->GetStatesForAllPermissions();
  for (NSNumber* key in statesForAllPermissions) {
    web::Permission permission =
        static_cast<web::Permission>(key.unsignedIntValue);
    web::PermissionState state =
        (web::PermissionState)statesForAllPermissions[key].unsignedIntValue;
    if ([self shouldShowPermission:permission withState:state]) {
      PermissionInfo* permissionInfo = [[PermissionInfo alloc] init];
      permissionInfo.permission = permission;
      permissionInfo.state = state;
      if (IsDomainLevelSitePermissionsEnabled()) {
        permissionInfo.setting = [self permissionSettingFor:permission];
      }
      [permissionsInfo addObject:permissionInfo];
    }
  }
  [self.consumer setPermissionsInfo:permissionsInfo];
}

// Returns whether `permission` with `state` should be displayed in Page Info.
- (BOOL)shouldShowPermission:(web::Permission)permission
                   withState:(web::PermissionState)state {
  if (state != web::PermissionStateNotAccessible) {
    return YES;
  }
  if (!_webState || !_hostContentSettingsMap ||
      !IsDomainLevelSitePermissionsEnabled()) {
    return NO;
  }
  const GURL& url = _webState->GetLastCommittedURL();
  if (!url.is_valid()) {
    return NO;
  }
  content_settings::SettingInfo settingInfo;
  _hostContentSettingsMap->GetWebsiteSetting(
      url, url, ContentSettingsTypeForPermission(permission), &settingInfo);
  return !settingInfo.primary_pattern.MatchesAllHosts();
}

// Resolves the current domain-level permission setting for `permission`.
- (SitePermissionSetting)permissionSettingFor:(web::Permission)permission {
  if (!_webState || !_hostContentSettingsMap) {
    return SitePermissionSetting::kAllowOnce;
  }
  if (_webState->GetStateForPermission(permission) ==
      web::PermissionStateBlocked) {
    return SitePermissionSetting::kNeverAllow;
  }
  const GURL& url = _webState->GetLastCommittedURL();
  if (!url.is_valid()) {
    return SitePermissionSetting::kAllowOnce;
  }
  ContentSettingsType contentType =
      ContentSettingsTypeForPermission(permission);
  ContentSetting setting =
      _hostContentSettingsMap->GetContentSetting(url, url, contentType);
  return SitePermissionSettingForContentSetting(setting);
}

// Persists `setting` for `permission` to `HostContentSettingsMap`.
- (void)persistSetting:(SitePermissionSetting)setting
         forPermission:(web::Permission)permission {
  if (!_hostContentSettingsMap || !_webState) {
    return;
  }
  const GURL& url = _webState->GetLastCommittedURL();
  if (!url.is_valid()) {
    return;
  }
  ContentSettingsType contentType =
      ContentSettingsTypeForPermission(permission);
  ContentSetting contentSetting =
      ContentSettingForSitePermissionSetting(setting);
  _hostContentSettingsMap->SetContentSettingDefaultScope(url, url, contentType,
                                                         contentSetting);
}

@end
