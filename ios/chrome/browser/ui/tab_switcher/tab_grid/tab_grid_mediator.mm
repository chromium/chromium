// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"

#import "base/metrics/histogram_macros.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"

@interface TabGridMediator () <PrefObserverDelegate>
@end

@implementation TabGridMediator {
  // Current selected grid.
  id<TabGridPageMutator> _currentPageMutator;
  PrefService* _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  [self notifyPageMutatorAboutPage:page];
}

- (void)setConsumer:(id<TabGridConsumer>)consumer {
  _consumer = consumer;
  [_consumer
      updateParentalControlStatus:supervised_user::IsSubjectToParentalControls(
                                      _prefService)];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kSupervisedUserId) {
    [_consumer updateParentalControlStatus:
                   supervised_user::IsSubjectToParentalControls(_prefService)];
  }
}

#pragma mark - Private

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

  // TODO(crbug.com/1462133): Implement the incognito grid or content visible
  // notification.
}

@end
