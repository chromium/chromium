// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

// TODO(crbug.com/384518419): Embed parent access widget into the bottom sheet
// view controller to display the appropriate web page.
@implementation ParentAccessCoordinator {
  ParentAccessCallbackCompletion _completion;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                completion:
                                    (ParentAccessCallbackCompletion)completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _completion = completion;
  }
  return self;
}

@end
