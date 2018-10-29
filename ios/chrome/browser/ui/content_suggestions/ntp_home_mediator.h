// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"

namespace ntp_snippets {
class ContentSuggestionsService;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ContentSuggestionsHeaderSynchronizer;
@class ContentSuggestionsMediator;
@class ContentSuggestionsMetricsRecorder;
@class ContentSuggestionsViewController;
@protocol LogoVendor;
@protocol NTPHomeConsumer;
@class NTPHomeMetrics;
class TemplateURLService;
@protocol SnackbarCommands;
@protocol UrlLoader;
class WebStateList;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NTPHomeMediator
    : NSObject<ContentSuggestionsCommands,
               ContentSuggestionsGestureCommands,
               ContentSuggestionsHeaderViewControllerDelegate>

- (nullable instancetype)
initWithWebStateList:(nonnull WebStateList*)webStateList
  templateURLService:(nonnull TemplateURLService*)templateURLService
          logoVendor:(nonnull id<LogoVendor>)logoVendor
    NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)init NS_UNAVAILABLE;

// Dispatcher.
@property(nonatomic, weak, nullable)
    id<ApplicationCommands, BrowserCommands, SnackbarCommands, UrlLoader>
        dispatcher;
// Suggestions service used to get the suggestions.
@property(nonatomic, assign, nonnull)
    ntp_snippets::ContentSuggestionsService* suggestionsService;
// Recorder for the metrics related to ContentSuggestions.
@property(nonatomic, strong, nullable)
    ContentSuggestionsMetricsRecorder* metricsRecorder;
// Recorder for the metrics related to the NTP.
@property(nonatomic, strong, nullable) NTPHomeMetrics* NTPMetrics;
// View Controller displaying the suggestions.
@property(nonatomic, weak, nullable)
    ContentSuggestionsViewController* suggestionsViewController;
@property(nonatomic, weak, nullable)
    ContentSuggestionsHeaderSynchronizer* headerCollectionInteractionHandler;
// Mediator for the ContentSuggestions.
@property(nonatomic, strong, nonnull)
    ContentSuggestionsMediator* suggestionsMediator;
// Consumer for this mediator.
@property(nonatomic, weak, nullable) id<NTPHomeConsumer> consumer;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_MEDIATOR_H_
