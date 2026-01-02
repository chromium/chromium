// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_mediator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/gemini/ui/assistant_gemini_consumer.h"
#import "ios/chrome/browser/assistant/ui/assistant_bar_configuration.h"
#import "ios/chrome/browser/assistant/ui/assistant_bar_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {
constexpr CGFloat kSymbolPointSize = 18.0;
}  // namespace

@implementation AssistantGeminiMediator {
  AssistantBarConfiguration* _barConfiguration;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _barConfiguration = [[AssistantBarConfiguration alloc] init];
  }
  return self;
}

#pragma mark - Properties

- (void)setHandler:(id<AssistantCommands>)handler {
  _handler = handler;
  if (_handler) {
    [self updateBarConfiguration];
  }
}

#pragma mark - Private

// Updates the bar configuration and notifies the handler.
- (void)updateBarConfiguration {
  // TODO(crbug.com/469050167): Localize.
  _barConfiguration.title = @"Gemini Assistant";
  _barConfiguration.leadingButtons = @[];

  // TODO(crbug.com/469050167): Remove these fake buttons once real logic is
  // available.
  __weak __typeof(self) weakSelf = self;
  AssistantBarItem* settingsButton = [[AssistantBarItem alloc]
           initWithImage:DefaultSymbolWithPointSize(kSettingsSymbol,
                                                    kSymbolPointSize)
      accessibilityLabel:@"Settings"
                  action:^{
                    [weakSelf didTapSettings];
                  }];

  _barConfiguration.trailingButtons = @[ settingsButton ];

  [self.handler updateBarConfiguration:_barConfiguration];
}

// Called when the settings button is tapped.
- (void)didTapSettings {
  // Update content of the sheet (fake action).
  [self.consumer
      setContentText:@"Settings button tapped! (Updated by Mediator)"];

  // Update navbar to show a leading button (Back).
  _barConfiguration.title = @"Settings";

  __weak __typeof(self) weakSelf = self;
  AssistantBarItem* backButton = [[AssistantBarItem alloc]
           initWithImage:DefaultSymbolWithPointSize(kChevronBackwardSymbol,
                                                    kSymbolPointSize)
      accessibilityLabel:@"Back"
                  action:^{
                    [weakSelf didTapBack];
                  }];
  _barConfiguration.leadingButtons = @[ backButton ];

  [self.handler updateBarConfiguration:_barConfiguration];
}

// Called when the back button is tapped.
- (void)didTapBack {
  [self.consumer setContentText:@"Back tapped! Reverting to initial state."];
  [self updateBarConfiguration];
}

@end
