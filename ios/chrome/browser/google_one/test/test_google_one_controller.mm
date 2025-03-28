// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/test/test_google_one_controller.h"

#import "ios/chrome/browser/google_one/test/constants.h"

@implementation TestGoogleOneController {
  UIViewController* _viewController;
}

- (void)launchWithViewController:(UIViewController*)baseViewController
                      completion:(void (^)(NSError*))completion {
  _viewController = [[UIViewController alloc] init];
  _viewController.view.backgroundColor = [UIColor redColor];
  _viewController.view.accessibilityIdentifier =
      kTestGoogleOneControllerAccessibilityID;
  [baseViewController presentViewController:_viewController
                                   animated:NO
                                 completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
}

@end

@implementation TestGoogleOneControllerFactory

+ (instancetype)sharedInstance {
  static TestGoogleOneControllerFactory* instance =
      [[TestGoogleOneControllerFactory alloc] init];
  return instance;
}

- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration {
  return [[TestGoogleOneController alloc] init];
}
@end
