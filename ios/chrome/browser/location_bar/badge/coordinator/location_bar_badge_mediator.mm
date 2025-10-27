// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import "base/timer/timer.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/ui/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"

namespace {

// Time to transition in seconds.
const int kTransitionTimeInSeconds = 2;

}  // anonymous namespace

@implementation LocationBarBadgeMediator {
  // Timer keeping track of when badge transitions to a promo.
  std::unique_ptr<base::OneShotTimer> _promoStartTimer;
  // Timer keeping track of when to return to the default badge state after
  // a promo was shown.
  std::unique_ptr<base::OneShotTimer> _promoEndTimer;
}

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kBadgeView hidden:hidden];
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kIncognito hidden:hidden];
}

#pragma mark - ReaderModeChipVisibilityDelegate

- (void)readerModeChipCoordinator:(ReaderModeChipCoordinator*)coordinator
       didSetReaderModeChipHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kReaderMode hidden:hidden];
}

#pragma mark - LocationBarBadgeCommands

- (void)updateBadgeConfig:(LocationBarBadgeConfiguration*)config {
  [self.consumer setBadgeConfig:config];
  [self.consumer transitionToSmallEntrypoint];
  [self.consumer showEntrypoint];
}

#pragma mark - Private

// Starts the promo timer.
- (void)startPromoTimer {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoStartTimer = std::make_unique<base::OneShotTimer>();
  _promoStartTimer->Start(FROM_HERE, base::Seconds(kTransitionTimeInSeconds),
                          base::BindOnce(^{
                            [weakSelf setupAndExpandChip];
                          }));
}

// Transforms the badge into a chip and starts the timers to transition back to
// the default badge state.
- (void)setupAndExpandChip {
  if (![self.delegate canShowLargeContextualPanelEntrypoint:self]) {
    // Enable fullscreen in case it was disabled when trying to show the IPH.
    [self.delegate enableFullscreen];
    return;
  }

  [self.delegate disableFullscreen];
  [self.consumer transitionToLargeEntrypoint];

  // TODO(crbug.com/454072799): Add metric log for chip showing.

  __weak LocationBarBadgeMediator* weakSelf = self;

  _promoEndTimer = std::make_unique<base::OneShotTimer>();
  _promoEndTimer->Start(FROM_HERE, base::Seconds(kTransitionTimeInSeconds),
                        base::BindOnce(^{
                          [weakSelf cleanupAndTransitionToDefaultBadgeState];
                        }));
}

// Changes the UI to the default badge state.
- (void)cleanupAndTransitionToDefaultBadgeState {
  [self.consumer transitionToSmallEntrypoint];
  [self.delegate enableFullscreen];
}

@end
