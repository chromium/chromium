// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"

#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoPermissionsMediator ()

@property(nonatomic, assign) web::WebState* webState;
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, NSNumber*>* accessiblePermissionStates;

@end

@implementation PageInfoPermissionsMediator

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  self.accessiblePermissionStates = [[NSMutableDictionary alloc] init];

  if (web::features::IsMediaPermissionsControlEnabled()) {
    NSDictionary<NSNumber*, NSNumber*>* statesForAllPermissions =
        webState->GetStatesForAllPermissions();
    for (NSNumber* key in statesForAllPermissions) {
      web::PermissionState state =
          (web::PermissionState)statesForAllPermissions[key].unsignedIntValue;
      switch (state) {
        case web::PermissionStateNotAccessible:
          break;
        case web::PermissionStateBlocked:
          self.accessiblePermissionStates[key] = @NO;
          break;
        case web::PermissionStateAllowed:
          self.accessiblePermissionStates[key] = @YES;
          break;
      }
    }
    self.webState = webState;
  }
  return self;
}

#pragma mark - PageInfoViewControllerPermissionsDelegate

- (BOOL)shouldShowPermissionsSection {
  return [self.accessiblePermissionStates count] > 0;
}

- (BOOL)isPermissionAccessible:(web::Permission)permission {
  return self.accessiblePermissionStates[@(permission)] != nil;
}

- (BOOL)stateForAccessiblePermission:(web::Permission)permission {
  return self.accessiblePermissionStates[@(permission)].boolValue;
}

- (void)toggleStateForPermission:(web::Permission)permission {
  if ([self isPermissionAccessible:permission]) {
    BOOL newValue = ![self stateForAccessiblePermission:permission];
    web::PermissionState state =
        newValue ? web::PermissionStateAllowed : web::PermissionStateBlocked;
    self.webState->SetStateForPermission(state, permission);
    self.accessiblePermissionStates[@(permission)] = @(newValue);
  }
}

@end
