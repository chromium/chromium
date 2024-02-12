// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Browser;
@class ContentSuggestionsMetricsRecorder;
@class MagicStackRankingModel;
@class MostVisitedTilesMediator;
@protocol NewTabPageMetricsDelegate;
@class SetUpListMediator;
@class ShortcutsMediator;

// Mediator for ContentSuggestions.
@interface ContentSuggestionsMediator
    : NSObject <ContentSuggestionsCommands, StartSurfaceRecentTabObserving>

// Default initializer.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

@property(nonatomic, weak) MagicStackRankingModel* magicStackRankingModel;

// The consumer that will be notified when the data change.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// Delegate for reporting content suggestions actions to the NTP metrics
// recorder.
@property(nonatomic, weak) id<NewTabPageMetricsDelegate> NTPMetricsDelegate;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

@property(nonatomic, weak) MostVisitedTilesMediator* mostVisitedTilesMediator;
@property(nonatomic, weak) SetUpListMediator* setUpListMediator;
@property(nonatomic, weak) ShortcutsMediator* shortcutsMediator;

// Disconnects the mediator.
- (void)disconnect;

// Whether the most recent tab tile is being shown. Returns YES if
// configureMostRecentTabItemWithWebState: has been called.
- (BOOL)mostRecentTabStartSurfaceTileIsShowing;

// Configures the most recent tab item with `webState` and `timeLabel`.
- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState
                                     timeLabel:(NSString*)timeLabel;

// Indicates that the "Return to Recent Tab" tile should be hidden.
- (void)hideRecentTabTile;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
