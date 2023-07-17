// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridMediator {
  // Current selected grid.
  id<TabGridPageMutator> _currentPageMutator;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  [self notifyPageMutatorAboutPage:page];
}

#pragma mark - Private

// Records when the user switches between incognito, regular or remotes pages in
// the tab grid.
// TODO(crbug.com/1457146): Move the logic to incognito, regular and remote
// mediators once it is created. Each grid will be responsible for their own
// logs.
- (void)recordActionSwitchingToPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectIncognitoPanel"));
      break;
    case TabGridPageRegularTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectRegularPanel"));
      break;
    case TabGridPageRemoteTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectRemotePanel"));
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
      break;
  }
}

// Sets the current selected mutator according to the page.
- (void)updateCurrentPageMutatorForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      _currentPageMutator = self.incognitoPageMutator;
      break;
    case TabGridPageRegularTabs:
      _currentPageMutator = self.regularPageMutator;
      break;
    case TabGridPageRemoteTabs:
      _currentPageMutator = self.remotePageMutator;
      break;
  }
}

// Notifies mutators if it is the current selected one or not.
- (void)notifyPageMutatorAboutPage:(TabGridPage)page {
  [_currentPageMutator currentlySelectedGrid:NO];
  [self updateCurrentPageMutatorForPage:page];
  [_currentPageMutator currentlySelectedGrid:YES];
}

#pragma mark - TabGridMutator

- (void)pageChanged:(TabGridPage)currentPage
        interaction:(TabSwitcherPageChangeInteraction)interaction {
  UMA_HISTOGRAM_ENUMERATION(kUMATabSwitcherPageChangeInteractionHistogram,
                            interaction);

  [self notifyPageMutatorAboutPage:currentPage];

  // TODO(crbug.com/1457146): Send notification to the displayed grid when each
  // grid will have their own isolated objects.
  [self recordActionSwitchingToPage:currentPage];
  // TODO(crbug.com/1462133): Implement the incognito grid or content visible
  // notification.
}

@end
