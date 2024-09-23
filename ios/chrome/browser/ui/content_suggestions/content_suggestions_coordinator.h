// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class WebState;
}

@protocol ContentSuggestionsDelegate;
@class ContentSuggestionsMediator;
@class ContentSuggestionsViewController;
@protocol HomeCustomizationDelegate;
@protocol HomeStartDataSource;
@class MagicStackCollectionViewController;
@protocol NewTabPageControllerDelegate;
@protocol NewTabPageActionsDelegate;

// Coordinator to manage the Suggestions UI via a
// ContentSuggestionsViewController.
@interface ContentSuggestionsCoordinator : ChromeCoordinator

// Webstate associated with this coordinator.
@property(nonatomic, assign) web::WebState* webState;

@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// YES if the coordinator has started. If YES, start is a no-op.
@property(nonatomic, readonly) BOOL started;

// The ViewController that this coordinator managers.
@property(nonatomic, strong, readonly)
    ContentSuggestionsViewController* viewController;

// The Magic Stack UICollectionView.
@property(nonatomic, strong, readonly)
    MagicStackCollectionViewController* magicStackCollectionView;

// The mediator used by this coordinator.
// TODO(crbug.com/40251499): Replace this with a delegate to avoid exposing
// this.
@property(nonatomic, strong, readonly)
    ContentSuggestionsMediator* contentSuggestionsMediator;

// Delegate used to communicate Content Suggestions events to the delegate.
@property(nonatomic, weak) id<ContentSuggestionsDelegate> delegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Data Source for the Home Start state.
@property(nonatomic, weak) id<HomeStartDataSource> homeStartDataSource;

// Delegate for the Home Customization menu.
@property(nonatomic, weak) id<HomeCustomizationDelegate> customizationDelegate;

// Refreshes the contents owned by this coordinator.
- (void)refresh;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
