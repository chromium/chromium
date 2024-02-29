// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_browser_agent.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface ContextualPanelEntrypointMediator () <ContextualPanelCommands>
@end

@implementation ContextualPanelEntrypointMediator {
  // ContextualPanelBrowserAgent to retrieve entrypoint configurations.
  raw_ptr<ContextualPanelBrowserAgent> _contextualPanelBrowserAgent;
}

- (instancetype)initWithBrowserAgent:
    (ContextualPanelBrowserAgent*)browserAgent {
  self = [super init];
  if (self) {
    _contextualPanelBrowserAgent = browserAgent;
  }
  return self;
}

- (void)disconnect {
  _contextualPanelBrowserAgent = nullptr;
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)entrypointTapped {
  // Do something.
}

#pragma mark - ContextualPanelCommands

- (void)showContextualPanelEntrypoint {
  ContextualPanelItemConfiguration config =
      _contextualPanelBrowserAgent->GetEntrypointConfiguration();

  UIImage* image = DefaultSymbolWithPointSize(
      base::SysUTF8ToNSString(config.entrypoint_image_name),
      kInfobarSymbolPointSize);

  [self.consumer setEntrypointImage:image];
}

@end
