// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/ui_bundled/feed_management/feed_management_navigation_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"

namespace signin {
class IdentityManager;
}  // namespace signin
namespace web {
class WebState;
}  // namespace web

class AuthenticationService;
class ChromeAccountManagerService;
class DiscoverFeedService;
@protocol FeedControlDelegate;
@class FeedMetricsRecorder;
@protocol NewTabPageConsumer;
@protocol NewTabPageContentDelegate;
@protocol NewTabPageHeaderConsumer;
@class NewTabPageState;
class PrefService;
namespace syncer {
class SyncService;
}
class TemplateURLService;
class UrlLoadingBrowserAgent;
@protocol UserAccountImageUpdateDelegate;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NewTabPageMediator
    : NSObject <FeedManagementNavigationDelegate, NewTabPageMutator>

- (instancetype)
    initWithTemplateURLService:(TemplateURLService*)templateURLService
                     URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                   authService:(AuthenticationService*)authService
               identityManager:(signin::IdentityManager*)identityManager
         accountManagerService:
             (ChromeAccountManagerService*)accountManagerService
      identityDiscImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater
                   isIncognito:(BOOL)isIncognito
           discoverFeedService:(DiscoverFeedService*)discoverFeedService
                   prefService:(PrefService*)prefService
                   syncService:(syncer::SyncService*)syncService
                    isSafeMode:(BOOL)isSafeMode NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Recorder for the metrics related to the feed.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;
// Consumer for this mediator.
@property(nonatomic, weak) id<NewTabPageConsumer> consumer;
// Consumer for NTP header model updates.
@property(nonatomic, weak) id<NewTabPageHeaderConsumer> headerConsumer;
// Delegate for controlling the current feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;
// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> NTPContentDelegate;
// Indicates whether the feed header should be visible.
@property(nonatomic, readonly, getter=isFeedHeaderVisible)
    BOOL feedHeaderVisible;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

// Saves the current state of the NTP.
- (void)saveNTPStateForWebState:(web::WebState*)webState;

// Restores the current state of the NTP.
- (void)restoreNTPStateForWebState:(web::WebState*)webState;

// Handles the actions following a tap on the "Learn More" item in the Discover
// feed menu.
- (void)handleFeedLearnMoreTapped;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_MEDIATOR_H_
