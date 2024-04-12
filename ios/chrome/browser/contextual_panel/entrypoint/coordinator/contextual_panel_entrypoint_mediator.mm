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
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface ContextualPanelEntrypointMediator () <
    ContextualPanelEntrypointCommands>
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

#pragma mark - ContextualPanelEntrypointCommands

- (void)updateContextualPanelEntrypointForNewModelData {
  _transitionToLargeEntrypointTimer = nullptr;
  _transitionToSmallEntrypointTimer = nullptr;

  [self.delegate enableFullscreen];

  if (!_contextualPanelBrowserAgent
           ->IsEntrypointConfigurationAvailableForCurrentTab()) {
    [self.consumer hideEntrypoint];
    return;
  }

  base::WeakPtr<ContextualPanelItemConfiguration> config =
      _contextualPanelBrowserAgent->GetEntrypointConfigurationForCurrentTab();

  [self.consumer setEntrypointConfig:config];
  [self.consumer transitionToSmallEntrypoint];
  [self.consumer showEntrypoint];

  if (![self canShowLargeEntrypointWithConfig:config]) {
    return;
  }

  // Start timers since we can show the large entrypoint.
  __weak ContextualPanelEntrypointMediator* weakSelf = self;

  // TODO(crbug.com/330702363): Make amount of time Finchable.
  _transitionToLargeEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToLargeEntrypointTimer->Start(
      FROM_HERE, base::Seconds(3), base::BindOnce(^{
        ContextualPanelEntrypointMediator* strongSelf = weakSelf;

        if (![strongSelf.delegate
                canShowLargeContextualPanelEntrypoint:strongSelf]) {
          return;
        }

        strongSelf->_contextualPanelBrowserAgent
            ->SetLargeEntrypointShownForCurrentTab(true);
        [strongSelf.delegate disableFullscreen];
        [strongSelf.consumer transitionToLargeEntrypoint];
      }));

  _transitionToSmallEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToSmallEntrypointTimer->Start(
      FROM_HERE, base::Seconds(8), base::BindOnce(^{
        [weakSelf.consumer transitionToSmallEntrypoint];
        [weakSelf.delegate enableFullscreen];
      }));
}

#pragma mark - private

- (BOOL)canShowLargeEntrypointWithConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  return !_contextualPanelBrowserAgent
              ->WasLargeEntrypointShownForCurrentTab() &&
         !config->entrypoint_message.empty() &&
         config->relevance >= config->high_relevance &&
         [self.delegate canShowLargeContextualPanelEntrypoint:self];
}

@end
