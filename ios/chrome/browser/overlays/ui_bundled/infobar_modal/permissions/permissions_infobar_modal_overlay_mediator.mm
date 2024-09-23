// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/permissions/permissions_infobar_modal_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_metrics_util.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

@interface PermissionsInfobarModalOverlayMediator () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

// The permissions modal config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
// The observed webState.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation PermissionsInfobarModalOverlayMediator

#pragma mark - Public

- (void)setConsumer:(id<PermissionsConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!_consumer || !config) {
    return;
  }

  PermissionsInfobarDelegate* delegate =
      static_cast<PermissionsInfobarDelegate*>(config->delegate());
  self.webState = delegate->GetWebState();

  if (!self.webState) {
    return;
  }

  _observer = std::make_unique<web::WebStateObserverBridge>(self);
  self.webState->AddObserver(_observer.get());

  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  const GURL& URL = visibleItem->GetURL();

  NSString* permissionsDescription =
      l10n_util::GetNSStringF(IDS_IOS_PERMISSIONS_INFOBAR_MODAL_DESCRIPTION,
                              base::UTF8ToUTF16(URL.host()));

  if ([_consumer respondsToSelector:@selector(setPermissionsDescription:)]) {
    [_consumer setPermissionsDescription:permissionsDescription];
  }
  [self dispatchInitialPermissionsInfo];
}

- (void)disconnect {
  [self detachFromWebState];
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didChangeStateForPermission:(web::Permission)permission {
  PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
  permissionsDescription.permission = permission;
  permissionsDescription.state =
      self.webState->GetStateForPermission(permission);
  [self.consumer permissionStateChanged:permissionsDescription];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self detachFromWebState];
}

#pragma mark - PermissionsDelegate

- (void)updateStateForPermission:(PermissionInfo*)permissionDescription {
  RecordPermissionToogled();
  self.webState->SetStateForPermission(permissionDescription.state,
                                       permissionDescription.permission);
  RecordPermissionEventFromOrigin(
      permissionDescription,
      PermissionEventOrigin::PermissionEventOriginModalInfobar);
}

#pragma mark - Private

// Helper that creates and dispatches initial permissions information to the
// InfobarModal.
- (void)dispatchInitialPermissionsInfo {
  NSMutableDictionary<NSNumber*, NSNumber*>* permissionsInfo =
      [[NSMutableDictionary alloc] init];

  NSDictionary<NSNumber*, NSNumber*>* statesForAllPermissions =
      self.webState->GetStatesForAllPermissions();
  for (NSNumber* key in statesForAllPermissions) {
    web::PermissionState state =
        (web::PermissionState)statesForAllPermissions[key].unsignedIntValue;
    if (state != web::PermissionStateNotAccessible) {
      [permissionsInfo setObject:statesForAllPermissions[key] forKey:key];
    }
  }
  [self.consumer setPermissionsInfo:permissionsInfo];
}

- (void)detachFromWebState {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
}

@end
