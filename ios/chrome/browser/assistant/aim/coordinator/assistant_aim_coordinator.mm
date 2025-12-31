// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"

#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_consumer.h"

@implementation AssistantAIMCoordinator

@synthesize viewController = _viewController;

- (void)start {
  _viewController = [[AssistantAIMViewController alloc] init];
}

- (void)stop {
  _viewController = nil;
}

- (AssistantBarConfiguration*)barConfiguration {
  AssistantBarConfiguration* config = [[AssistantBarConfiguration alloc] init];
  // TODO(crbug.com/469050167): Localize.
  config.title = @"AI Assistant";
  return config;
}

@end
