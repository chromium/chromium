// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_commands.h"

namespace favicon {
class LargeIconService;
}

namespace ntp_tiles {
class MostVisitedSites;
}

@class BrowserActionFactory;
@protocol ContentSuggestionsConsumer;
@protocol ContentSuggestionsDelegate;
enum class ContentSuggestionsModuleType;
@class ContentSuggestionsMetricsRecorder;
class LargeIconCache;
@class MostVisitedTilesConfig;
@protocol NewTabPageActionsDelegate;
class PrefService;
@protocol SnackbarCommands;
class UrlLoadingBrowserAgent;

// Delegate used to communicate events back to the owner of
// MostVisitedTilesMediator.
@protocol MostVisitedTilesMediatorDelegate

// Logs a user Magic Stack engagement for module `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

// Indicates to the receiver that the initial Most Visited Tiles list has been
// received.
- (void)didReceiveInitialMostVistedTiles;

// Indicates to the receiver to remove the MostVisitedTiles module.
- (void)removeMostVisitedTilesModule;

@end

// Mediator for managing the state of the MostVisitedTiles for the Magic Stack
// module.
@interface MostVisitedTilesMediator
    : NSObject <ContentSuggestionsImageDataSource, MostVisitedTilesCommands>

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

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<MostVisitedTilesMediatorDelegate> delegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Dispatcher.
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

// Default initializer.
- (instancetype)
    initWithMostVisitedSite:
        (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
                prefService:(PrefService*)prefService
           largeIconService:(favicon::LargeIconService*)largeIconService
             largeIconCache:(LargeIconCache*)largeIconCache
     URLLoadingBrowserAgent:(UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Trigger a refresh of the Most Visited tiles.
- (void)refreshMostVisitedTiles;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_MEDIATOR_H_
