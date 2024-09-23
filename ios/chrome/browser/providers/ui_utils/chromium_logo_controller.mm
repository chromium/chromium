// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/ui_utils/chromium_logo_controller.h"

#import <UIKit/UIKit.h>

@implementation ChromiumLogoController

@synthesize doodleObserver = _doodleObserver;
@synthesize showingLogo = _showingLogo;
@synthesize usesMonochromeLogo = _usesMonochromeLogo;
@synthesize view = _view;

- (instancetype)init {
  if ((self = [super init])) {
    _view = [[UIView alloc] init];
  }
  return self;
}

- (void)fetchDoodle {
  // Do nothing.
}

- (BOOL)isShowingDoodle {
  return false;
}

- (void)setWebState:(web::WebState*)webState {
  // Do nothing.
}

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return nil;
}

@end
