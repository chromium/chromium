// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"

#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_metrics_util.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

@interface PageInfoPermissionsMediator () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

@property(nonatomic, assign) web::WebState* webState;
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, NSNumber*>* accessiblePermissionStates;

@end

@implementation PageInfoPermissionsMediator

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState;
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_observer.get());
  }
  return self;
}

- (void)setConsumer:(id<PermissionsConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;
  [self dispatchInitialPermissionsInfo];
}

- (void)disconnect {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
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
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
}

#pragma mark - PermissionsDelegate

- (void)updateStateForPermission:(PermissionInfo*)permissionDescription {
  RecordPermissionToogled();
  self.webState->SetStateForPermission(permissionDescription.state,
                                       permissionDescription.permission);
  RecordPermissionEventFromOrigin(
      permissionDescription,
      PermissionEventOrigin::PermissionEventOriginPageInfo);
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

@end
