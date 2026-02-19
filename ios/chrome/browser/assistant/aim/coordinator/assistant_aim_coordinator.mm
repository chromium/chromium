// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"
#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"

@implementation AssistantAIMCoordinator {
  AssistantAIMViewController* _viewController;
}

- (UIViewController*)viewController {
  return _viewController;
}

- (void)start {
  _viewController = [[AssistantAIMViewController alloc] init];
}

- (void)stop {
  _viewController = nil;
}

@end
