// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"

namespace signin {
class IdentityManager;
}

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace web {
class WebState;
}

@protocol ApplicationCommands;
class AuthenticationService;
class Browser;
@protocol BrowserCommands;
@protocol ContentSuggestionsCollectionControlling;
@class ContentSuggestionsHeaderSynchronizer;
@class ContentSuggestionsMediator;
@class ContentSuggestionsMetricsRecorder;
@class ContentSuggestionsViewController;
@protocol LogoVendor;
@protocol NewTabPageFeedDelegate;
@class NewTabPageViewController;
@protocol NTPHomeConsumer;
@class NTPHomeMetrics;
@class DiscoverFeedMetricsRecorder;
@protocol OmniboxCommands;
class TemplateURLService;
@protocol SnackbarCommands;
class UrlLoadingBrowserAgent;
class VoiceSearchAvailability;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NTPHomeMediator
    : NSObject<ContentSuggestionsCommands,
               ContentSuggestionsGestureCommands,
               ContentSuggestionsHeaderViewControllerDelegate>

- (instancetype)initWithWebState:(web::WebState*)webState
              templateURLService:(TemplateURLService*)templateURLService
                       URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                     authService:(AuthenticationService*)authService
                 identityManager:(signin::IdentityManager*)identityManager
                      logoVendor:(id<LogoVendor>)logoVendor
         voiceSearchAvailability:
             (VoiceSearchAvailability*)voiceSearchAvailability
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Dispatcher.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, OmniboxCommands, SnackbarCommands>
        dispatcher;
// Suggestions service used to get the suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* suggestionsService;
// Recorder for the metrics related to ContentSuggestions.
@property(nonatomic, strong) ContentSuggestionsMetricsRecorder* metricsRecorder;
// Recorder for the metrics related to the NTP.
@property(nonatomic, strong) NTPHomeMetrics* NTPMetrics;
// Recorder for the metrics related to the Discover feed.
@property(nonatomic, strong) DiscoverFeedMetricsRecorder* discoverFeedMetrics;
// Primary collection view controller that receives scroll events.
// In the refactored NTP, the Discover feed collection view behaves as the
// primary NTP scroll view. Otherwise, the content suggestions collection view
// becomes the main NTP scroll view.
// TODO(crbug.com/1114792): Change this comment to remove the mention of the
// refactored NTP.
@property(nonatomic, weak) id<ContentSuggestionsCollectionControlling>
    primaryViewController;
// View Controller for the NTP if using the non refactored NTP or the Feed is
// not visible.
// TODO(crbug.com/1114792): Create a protocol to avoid duplication and update
// comment.
@property(nonatomic, weak)
    ContentSuggestionsViewController* suggestionsViewController;
// View Controller forthe NTP if using the refactored NTP and the Feed is
// visible.
// TODO(crbug.com/1114792): Create a protocol to avoid duplication and update
// comment.
@property(nonatomic, weak) NewTabPageViewController* ntpViewController;
@property(nonatomic, weak)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;
// Mediator for the ContentSuggestions.
@property(nonatomic, strong) ContentSuggestionsMediator* suggestionsMediator;
// Consumer for this mediator.
@property(nonatomic, weak) id<NTPHomeConsumer> consumer;
// The browser.
@property(nonatomic, assign) Browser* browser;

// Delegate for providing information relating to the feed.
@property(nonatomic, weak) id<NewTabPageFeedDelegate> ntpFeedDelegate;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

// The location bar has lost focus.
- (void)locationBarDidResignFirstResponder;

// Tell location bar has taken focus.
- (void)locationBarDidBecomeFirstResponder;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
