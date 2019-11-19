// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ContentSuggestionsHeaderViewController;
@protocol NewTabPageControllerDelegate;
@protocol OmniboxFocuser;
@protocol FakeboxFocuser;
@protocol SnackbarCommands;

// Coordinator to manage the Suggestions UI via a
// ContentSuggestionsViewController.
@interface ContentSuggestionsCoordinator : ChromeCoordinator

// BrowserState used to create the ContentSuggestionFactory.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
// URLLoader used to open pages.
@property(nonatomic, assign) web::WebState* webState;
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              OmniboxFocuser,
                              FakeboxFocuser,
                              SnackbarCommands>
    dispatcher;
// Whether the Suggestions UI is displayed. If this is true, start is a no-op.
@property(nonatomic, readonly) BOOL visible;

@property(nonatomic, strong, readonly)
    ContentSuggestionsHeaderViewController* headerController;

@property(nonatomic, strong, readonly)
    UICollectionViewController* viewController;

// Dismisses all modals owned by the NTP mediator.
- (void)dismissModals;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// The content inset and offset of the scroll view.
- (UIEdgeInsets)contentInset;
- (CGPoint)contentOffset;

// The current NTP view.
- (UIView*)view;

// Reloads the suggestions.
- (void)reload;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
