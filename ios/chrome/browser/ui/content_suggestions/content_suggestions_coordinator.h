// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace web {
class WebState;
}

@class ContentSuggestionsViewController;
@protocol FeedDelegate;
@protocol NewTabPageControllerDelegate;
@protocol NewTabPageDelegate;
@class NTPHomeMediator;
@protocol ThumbStripSupporting;
@class ViewRevealingVerticalPanHandler;

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

// Allows for the in-flight enabling/disabling of the thumb strip.
@property(nonatomic, weak, readonly) id<ThumbStripSupporting>
    thumbStripSupporting;

// NTP Mediator used by this Coordinator.
// TODO(crbug.com/1114792): Move all usage of this mediator to NTPCoordinator.
// It might also be necessary to split it and create a ContentSuggestions
// mediator for non NTP logic.
@property(nonatomic, strong) NTPHomeMediator* ntpMediator;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> ntpDelegate;

// Delegate used to communicate to communicate events to the feed.
@property(nonatomic, weak) id<FeedDelegate> feedDelegate;

// Reloads the suggestions.
- (void)reload;

// The location bar has lost focus.
- (void)locationBarDidResignFirstResponder;

// Tell location bar has taken focus.
- (void)locationBarDidBecomeFirstResponder;

// Configure Content Suggestions if showing the Start Surface.
- (void)configureStartSurfaceIfNeeded;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
