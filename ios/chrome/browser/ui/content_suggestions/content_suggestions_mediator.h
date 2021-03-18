// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

namespace favicon {
class LargeIconService;
}

namespace ntp_snippets {
class ContentSuggestionsService;
}

namespace ntp_tiles {
class MostVisitedSites;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsConsumer;
@protocol ContentSuggestionsGestureCommands;
@protocol ContentSuggestionsHeaderProvider;
@class ContentSuggestionIdentifier;
@protocol DiscoverFeedDelegate;
class GURL;
class LargeIconCache;
class NotificationPromoWhatsNew;
class ReadingListModel;
class WebStateList;

// Mediator for ContentSuggestions. Makes the interface between a
// ntp_snippets::ContentSuggestionsService and the Objective-C services using
// its data.
@interface ContentSuggestionsMediator
    : NSObject <ContentSuggestionsDataSource,
                ContentSuggestionsMetricsRecorderDelegate,
                StartSurfaceRecentTabObserving>

// Initialize the mediator with the |contentService| to mediate.
- (instancetype)
           initWithContentService:
               (ntp_snippets::ContentSuggestionsService*)contentService
                 largeIconService:(favicon::LargeIconService*)largeIconService
                   largeIconCache:(LargeIconCache*)largeIconCache
                  mostVisitedSite:(std::unique_ptr<ntp_tiles::MostVisitedSites>)
                                      mostVisitedSites
                 readingListModel:(ReadingListModel*)readingListModel
                      prefService:(PrefService*)prefService
                     discoverFeed:(UIViewController*)discoverFeed
    isGoogleDefaultSearchProvider:(BOOL)isGoogleDefaultSearchProvider
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Command handler for the mediator.
@property(nonatomic, weak)
    id<ContentSuggestionsCommands, ContentSuggestionsGestureCommands>
        commandHandler;

@property(nonatomic, weak) id<ContentSuggestionsHeaderProvider> headerProvider;

// Whether the contents section should be expanded or collapsed.  Collapsed
// means to show the header, but not any content or footer.
@property(nonatomic, strong) PrefBackedBoolean* contentArticlesExpanded;
// Whether to force the reload the Reading List section next time it is updated.
// Reset to NO after actual reload.
@property(nonatomic, assign) BOOL readingListNeedsReload;

// ViewController created by the Discover provider containing the Discover feed.
@property(nonatomic, weak) UIViewController* discoverFeed;

// Delegate used to communicate to communicate events to the DiscoverFeed.
@property(nonatomic, weak) id<DiscoverFeedDelegate> discoverFeedDelegate;

// The consumer for this mediator.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// WebStateList associated with this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// Disconnects the mediator.
- (void)disconnect;

// Reloads content suggestions.
- (void)reloadAllData;

// The notification promo owned by this mediator.
- (NotificationPromoWhatsNew*)notificationPromo;

// Block |URL| from Most Visited sites.
- (void)blockMostVisitedURL:(GURL)URL;

// Always allow |URL| in Most Visited sites.
- (void)allowMostVisitedURL:(GURL)URL;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

// Configures the most recent tab item for |webState|.
- (void)configureMostRecentTabItemWithWebState:(web::WebState*)webState;

// Indicates that the "Return to Recent Tab" tile should be hidden.
- (void)hideRecentTabTile;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_MEDIATOR_H_
