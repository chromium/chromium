// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_commands.h"
#import "ios/chrome/browser/assistant/gemini/ui/assistant_gemini_view_controller.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_consumer.h"

@implementation AssistantGeminiCoordinator

@synthesize viewController = _viewController;

- (void)start {
  _viewController = [[AssistantGeminiViewController alloc] init];
}

- (void)stop {
  _viewController = nil;
}

- (AssistantBarConfiguration*)barConfiguration {
  AssistantBarConfiguration* config = [[AssistantBarConfiguration alloc] init];
  // TODO(crbug.com/469050167): Localize.
  config.title = @"Gemini";
  return config;
}

@end
