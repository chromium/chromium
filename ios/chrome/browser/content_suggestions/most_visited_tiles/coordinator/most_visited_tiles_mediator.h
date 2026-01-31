// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_COORDINATOR_MOST_VISITED_TILES_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_COORDINATOR_MOST_VISITED_TILES_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_commands.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_pinned_site_mutator.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_image_data_source.h"

namespace favicon {
class LargeIconService;
}  // namespace favicon

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace history {
class HistoryService;
}  // namespace history

namespace ntp_tiles {
class MostVisitedSites;
}  // namespace ntp_tiles

@class BrowserActionFactory;
class ChromeAccountManagerService;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsConsumer;
@protocol ContentSuggestionsDelegate;
enum class ContentSuggestionsModuleType;
@class ContentSuggestionsMetricsRecorder;
@protocol HelpCommands;
class LargeIconCache;
@class LayoutGuideCenter;
@class MostVisitedTilesConfig;
@protocol NewTabPageActionsDelegate;
class PrefService;
@protocol SnackbarCommands;
class UrlLoadingBrowserAgent;

// Mediator for managing the state of the MostVisitedTiles for the Magic Stack
// module.
@interface MostVisitedTilesMediator
    : NSObject <ContentSuggestionsImageDataSource,
                MostVisitedTilesCommands,
                MostVisitedTilesPinnedSiteMutator>

// The config object for the latest Most Visited Tiles.
@property(nonatomic, strong, readonly)
    MostVisitedTilesConfig* mostVisitedConfig;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// Action factory for mediator.
@property(nonatomic, strong) BrowserActionFactory* actionFactory;

// Consumer for this mediator.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// Delegate used to communicate Content Suggestions events to the delegate.
@property(nonatomic, weak) id<ContentSuggestionsDelegate>
    contentSuggestionsDelegate;

// Handler for content suggestion commands.
@property(nonatomic, weak) id<ContentSuggestionsCommands>
    contentSuggestionsHandler;

// Handler for snackbar commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

// Handler for in-product help commands.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

// Default initializer.
- (instancetype)
    initWithMostVisitedSite:
        (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
             historyService:(history::HistoryService*)historyService
                prefService:(PrefService*)prefService
           largeIconService:(favicon::LargeIconService*)largeIconService
             largeIconCache:(LargeIconCache*)largeIconCache
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
          engagementTracker:(feature_engagement::Tracker*)engagementTracker
          layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Trigger a refresh of the Most Visited tiles.
- (void)refreshMostVisitedTiles;

// Disable the most visited sites module.
- (void)disableModule;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_COORDINATOR_MOST_VISITED_TILES_MEDIATOR_H_
