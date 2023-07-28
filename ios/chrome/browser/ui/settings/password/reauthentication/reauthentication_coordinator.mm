// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReauthenticationCoordinator ()

// Module used for requesting Local Authentication.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

@end

@implementation ReauthenticationCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                          reauthenticationModule:(id<ReauthenticationProtocol>)
                                                     reauthenticationModule {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(reauthenticationModule);
    _reauthModule = reauthenticationModule;
  }

  return self;
}

@end
