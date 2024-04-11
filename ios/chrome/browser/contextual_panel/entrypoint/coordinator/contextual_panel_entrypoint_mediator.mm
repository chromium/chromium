// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
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

  // Timer keeping track of when to transition to a large entrypoint.
  std::unique_ptr<base::OneShotTimer> _transitionToLargeEntrypointTimer;

  // Timer to keep track of when to return to a small entrypoint after having
  // transitioned to a large entrypoint.
  std::unique_ptr<base::OneShotTimer> _transitionToSmallEntrypointTimer;
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

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  [self.delegate setLocationBarLabelCenteredBetweenContent:self
                                                  centered:centered];
}

#pragma mark - ContextualPanelCommands

- (void)showContextualPanelEntrypoint {
  base::WeakPtr<ContextualPanelItemConfiguration> config =
      _contextualPanelBrowserAgent->GetEntrypointConfiguration();

  [self.consumer setEntrypointConfig:config];
  [self.consumer showEntrypoint];

  if (![self.delegate canShowLargeContextualPanelEntrypoint:self]) {
    return;
  }

  // Start timers if we can show the large entrypoint.
  __weak ContextualPanelEntrypointMediator* weakSelf = self;

  // TODO(crbug.com/330702363): Make amount of time Finchable.
  _transitionToLargeEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToLargeEntrypointTimer->Start(
      FROM_HERE, base::Seconds(3), base::BindOnce(^{
        if (![weakSelf.delegate
                canShowLargeContextualPanelEntrypoint:weakSelf]) {
          return;
        }

        [weakSelf.consumer transitionToLargeEntrypoint];
      }));

  _transitionToSmallEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToSmallEntrypointTimer->Start(
      FROM_HERE, base::Seconds(8), base::BindOnce(^{
        [weakSelf.consumer transitionToSmallEntrypoint];
      }));
}

- (void)hideContextualPanelEntrypoint {
  _transitionToLargeEntrypointTimer = nullptr;
  _transitionToSmallEntrypointTimer = nullptr;
  [self.consumer hideEntrypoint];
}

@end
