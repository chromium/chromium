// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement
namespace image_fetcher {
class ImageFetcherService;
}  // namespace image_fetcher
namespace regional_capabilities {
class RegionalCapabilitiesService;
}  // namespace regional_capabilities
namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer
namespace web {
class WebState;
}  // namespace web

class AimEligibilityService;
class AuthenticationService;
class BrowserViewVisibilityNotifierBrowserAgent;
class ChromeAccountManagerService;
class DiscoverFeedService;
class DiscoverFeedVisibilityBrowserAgent;
@protocol DiscoverFeedVisibilityObserver;
@protocol FeedControlDelegate;
@class FeedMetricsRecorder;
class HomeBackgroundCustomizationService;
@protocol NewTabPageConsumer;
@protocol NewTabPageContentDelegate;
@protocol NewTabPageHeaderConsumer;
class PlaceholderService;
class PrefService;
class TemplateURLService;
class UrlLoadingBrowserAgent;
class UserUploadedImageManager;
@protocol UserAccountImageUpdateDelegate;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NewTabPageMediator : NSObject <NewTabPageMutator>

- (instancetype)
            initWithTemplateURLService:(TemplateURLService*)templateURLService
                             URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                           authService:(AuthenticationService*)authService
                       identityManager:(signin::IdentityManager*)identityManager
                 accountManagerService:
                     (ChromeAccountManagerService*)accountManagerService
              identityDiscImageUpdater:
                  (id<UserAccountImageUpdateDelegate>)imageUpdater
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
                           prefService:(PrefService*)prefService
                           syncService:(syncer::SyncService*)syncService
           regionalCapabilitiesService:
               (regional_capabilities::RegionalCapabilitiesService*)
                   regionalCapabilitiesService
        backgroundCustomizationService:
            (HomeBackgroundCustomizationService*)backgroundCustomizationService
                   imageFetcherService:
                       (image_fetcher::ImageFetcherService*)imageFetcherService
              userUploadedImageManager:
                  (UserUploadedImageManager*)userUploadedImageManager
         browserViewVisibilityNotifier:
             (BrowserViewVisibilityNotifierBrowserAgent*)
                 browserViewVisibilityNotifierBrowserAgent
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
              featureEngagementTracker:(feature_engagement::Tracker*)tracker
                 aimEligibilityService:
                     (AimEligibilityService*)aimEligibilityService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Recorder for the metrics related to the feed.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;
// Consumer for this mediator.
@property(nonatomic, weak) id<NewTabPageConsumer> consumer;
// Consumer for NTP header model updates.
@property(nonatomic, weak) id<NewTabPageHeaderConsumer> headerConsumer;
// Observer for feed visibility changes.
@property(nonatomic, weak) id<DiscoverFeedVisibilityObserver>
    feedVisibilityObserver;
// Placeholder service, for placeholder text and image.
@property(nonatomic, assign) PlaceholderService* placeholderService;
// Delegate for controlling the current feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;
// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> NTPContentDelegate;
// Indicates that the new tab page is visible.
@property(nonatomic, assign) BOOL NTPVisible;
// A pointer to the collection view that currently embeds all the contents on
// the new tab page.
@property(nonatomic, weak) UICollectionView* contentCollectionView;

// Indicates whether the feed header should be visible.
- (BOOL)isFeedHeaderVisible;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

// Saves the current scroll position of the NTP.
- (void)saveNTPScrollPositionForWebState:(web::WebState*)webState;

// Restores the current scroll position of the NTP.
- (void)restoreNTPScrollPositionForWebState:(web::WebState*)webState;

// Update the background of the NTP.
- (void)updateBackground;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_
