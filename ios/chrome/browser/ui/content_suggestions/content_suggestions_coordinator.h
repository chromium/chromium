// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class WebState;
}

@class ContentSuggestionsMediator;
@class ContentSuggestionsViewController;
@protocol FeedDelegate;
@protocol NewTabPageControllerDelegate;
@protocol NewTabPageDelegate;
@protocol NewTabPageMetricsDelegate;
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

// The mediator used by this coordinator.
// TODO(crbug.com/1403298): Replace this with a delegate to avoid exposing this.
@property(nonatomic, strong, readonly)
    ContentSuggestionsMediator* contentSuggestionsMediator;

// Allows for the in-flight enabling/disabling of the thumb strip.
@property(nonatomic, weak, readonly) id<ThumbStripSupporting>
    thumbStripSupporting;

// Delegate for NTP related actions.
@property(nonatomic, weak) id<NewTabPageDelegate> NTPDelegate;

// Delegate used to communicate to communicate events to the feed.
@property(nonatomic, weak) id<FeedDelegate> feedDelegate;

// Delegate for reporting content suggestions actions to the NTP metrics
// recorder.
@property(nonatomic, weak) id<NewTabPageMetricsDelegate> NTPMetricsDelegate;

// Reloads the suggestions.
- (void)reload;

// Configure Content Suggestions if showing the Start Surface. NOTE: this should
// only be called once for every Start configuration. Calling it multiple times
// in sequence can lead to unpredictable outcomes.
- (void)configureStartSurfaceIfNeeded;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COORDINATOR_H_
