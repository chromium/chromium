// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageCoordinator ()

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for incognito.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  return [super initWithBaseViewController:nil browserState:browserState];
}

- (void)start {
  if (self.started)
    return;

  DCHECK(self.browserState);
  DCHECK(self.webStateList);
  DCHECK(self.dispatcher);
  DCHECK(self.URLLoader);
  DCHECK(self.toolbarDelegate);

  if (self.browserState->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithLoader:self.URLLoader];
  } else {
    DCHECK(!self.contentSuggestionsCoordinator);
    self.contentSuggestionsCoordinator =
        [[ContentSuggestionsCoordinator alloc] initWithBaseViewController:nil];
    self.contentSuggestionsCoordinator.URLLoader = self.URLLoader;
    self.contentSuggestionsCoordinator.dispatcher = self.dispatcher;
    self.contentSuggestionsCoordinator.browserState = self.browserState;
    self.contentSuggestionsCoordinator.webStateList = self.webStateList;
    self.contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
    [self.contentSuggestionsCoordinator start];
    base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
  }
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.incognitoViewController = nil;
  self.started = NO;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  [self start];
  if (self.browserState->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.contentSuggestionsCoordinator.viewController;
  }
}

#pragma mark - Public Methods

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
  [self.incognitoViewController dismissModals];
}

#pragma mark - NewTabPageOwning

- (UIView*)view {
  return self.viewController.view;
}

- (CGPoint)scrollOffset {
  return [self.contentSuggestionsCoordinator scrollOffset];
}

- (void)willUpdateSnapshot {
  [self.contentSuggestionsCoordinator willUpdateSnapshot];
}

- (UIEdgeInsets)contentInset {
  return self.contentSuggestionsCoordinator.viewController.collectionView
      .contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  // UIKit will adjust the contentOffset sometimes when changing the
  // contentInset.bottom.  We don't want the NTP to scroll, so store and re-set
  // the contentOffset after setting the contentInset.
  CGPoint contentOffset = self.contentSuggestionsCoordinator.viewController
                              .collectionView.contentOffset;
  self.contentSuggestionsCoordinator.viewController.collectionView
      .contentInset = contentInset;
  self.contentSuggestionsCoordinator.viewController.collectionView
      .contentOffset = contentOffset;
}

- (void)focusFakebox {
  [self.contentSuggestionsCoordinator.headerController focusFakebox];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.contentSuggestionsCoordinator
              .headerController logoAnimationControllerOwner];
}

@end
