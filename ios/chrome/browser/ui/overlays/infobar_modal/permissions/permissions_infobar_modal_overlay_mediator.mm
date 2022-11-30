// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/permissions/permissions_infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/permissions/permissions_modal_overlay_request_config.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#import "ios/chrome/browser/ui/permissions/permission_metrics_util.h"
#import "ios/chrome/browser/ui/permissions/permissions_consumer.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PermissionsInfobarModalOverlayMediator () <CRWWebStateObserver> {
  std::unique_ptr<web::WebStateObserverBridge> _observer;
}

// The permissions modal config from the request.
@property(nonatomic, readonly)
    PermissionsInfobarModalOverlayRequestConfig* config;
// The observed webState.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation PermissionsInfobarModalOverlayMediator

#pragma mark - Public

- (void)setConsumer:(id<PermissionsConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  PermissionsInfobarModalOverlayRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  self.webState = self.config->GetWebState();
  _observer = std::make_unique<web::WebStateObserverBridge>(self);
  self.webState->AddObserver(_observer.get());

  if ([_consumer respondsToSelector:@selector(setPermissionsDescription:)]) {
    [_consumer
        setPermissionsDescription:self.config->GetPermissionsDescription()];
  }
  [self dispatchInitialPermissionsInfo];
}

- (void)disconnect {
  if (_webState && _observer) {
    _webState->RemoveObserver(_observer.get());
    _observer.reset();
    _webState = nullptr;
  }
}

#pragma mark - Accessors

- (PermissionsInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<PermissionsInfobarModalOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return PermissionsInfobarModalOverlayRequestConfig::RequestSupport();
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
  NSMutableArray<PermissionInfo*>* permissionsinfo =
      [[NSMutableArray alloc] init];

  NSDictionary<NSNumber*, NSNumber*>* statesForAllPermissions =
      self.webState->GetStatesForAllPermissions();
  for (NSNumber* key in statesForAllPermissions) {
    web::PermissionState state =
        (web::PermissionState)statesForAllPermissions[key].unsignedIntValue;
    if (state != web::PermissionStateNotAccessible) {
      PermissionInfo* permissionInfo = [[PermissionInfo alloc] init];
      permissionInfo.permission = (web::Permission)key.unsignedIntValue;
      permissionInfo.state = state;
      [permissionsinfo addObject:permissionInfo];
    }
  }
  [self.consumer setPermissionsInfo:permissionsinfo];
}

@end
