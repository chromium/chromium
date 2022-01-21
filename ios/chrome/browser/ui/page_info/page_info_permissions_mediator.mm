// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"

#import "ios/chrome/browser/ui/page_info/NSNumber+Permission.h"
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
    NSArray* permissionKeys = @[
      [NSNumber cr_numberWithPermission:web::Permission::CAMERA],
      [NSNumber cr_numberWithPermission:web::Permission::MICROPHONE]
    ];
    for (NSNumber* key in permissionKeys) {
      web::PermissionState state =
          webState->GetStateForPermission([key cr_permissionValue]);
      switch (state) {
        case web::PermissionState::NOT_ACCESSIBLE:
          break;
        case web::PermissionState::ALLOWED:
          self.accessiblePermissionStates[key] = @YES;
          break;
        case web::PermissionState::BLOCKED:
          self.accessiblePermissionStates[key] = @NO;
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
  return self.accessiblePermissionStates[
             [NSNumber cr_numberWithPermission:permission]] != nil;
}

- (BOOL)stateForAccessiblePermission:(web::Permission)permission {
  return [[self.accessiblePermissionStates
      objectForKey:[NSNumber cr_numberWithPermission:permission]] boolValue];
}

- (void)toggleStateForPermission:(web::Permission)permission {
  if ([self isPermissionAccessible:permission]) {
    BOOL newValue = ![self stateForAccessiblePermission:permission];
    web::PermissionState state = newValue ? web::PermissionState::ALLOWED
                                          : web::PermissionState::BLOCKED;
    self.webState->SetStateForPermission(state, permission);
    NSNumber* key = [NSNumber cr_numberWithPermission:permission];
    self.accessiblePermissionStates[key] = @(newValue);
  }
}

@end
