// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin_screen_view_controller.h"

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenViewController ()

@property(nonatomic, strong) IdentityButtonControl* identityControl;

@end

@implementation SigninScreenViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Test Sign-in Screen";
  self.primaryActionString = @"Test Continue Button";

  self.identityControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  [self.identityControl addTarget:self
                           action:@selector(identityButtonControlTapped:
                                                               forEvent:)
                 forControlEvents:UIControlEventTouchUpInside];

  // TODO(crbug.com/1189836): Add the identity control to the wrapper view.

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

#pragma mark - SignInScreenConsumer

- (void)setUserImage:(UIImage*)userImage {
  if (userImage) {
    [self.identityControl setIdentityAvatar:userImage];
  } else {
    // TODO(crbug.com/1189836): Update with default avatar.
  }
}

- (void)setSelectedIdentityUserName:(NSString*)userName email:(NSString*)email {
  self.identityControl.hidden = NO;
  [self.identityControl setIdentityName:userName email:email];
}

- (void)hideIdentityButtonControl {
  self.identityControl.hidden = YES;
}

#pragma mark - Private

- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate showAccountPickerFromPoint:[touch locationInView:nil]];
}

@end
