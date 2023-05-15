// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

namespace signin {
class IdentityManager;
}

namespace web {
class WebState;
}

class AuthenticationService;
class Browser;
class ChromeAccountManagerService;
@class ContentSuggestionsMediator;
@protocol FeedControlDelegate;
@class FeedMetricsRecorder;
class GURL;
@protocol LogoVendor;
@protocol NewTabPageConsumer;
@protocol NewTabPageHeaderConsumer;
class TemplateURLService;
class UrlLoadingBrowserAgent;
@protocol UserAccountImageUpdateDelegate;

// Mediator for the NTP Home panel, handling the interactions with the
// suggestions.
@interface NewTabPageMediator : NSObject

- (instancetype)
            initWithWebState:(web::WebState*)webState
          templateURLService:(TemplateURLService*)templateURLService
                   URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                 authService:(AuthenticationService*)authService
             identityManager:(signin::IdentityManager*)identityManager
       accountManagerService:(ChromeAccountManagerService*)accountManagerService
                  logoVendor:(id<LogoVendor>)logoVendor
    identityDiscImageUpdater:(id<UserAccountImageUpdateDelegate>)imageUpdater
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Recorder for the metrics related to the feed.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;
// Mediator for the ContentSuggestions.
// TODO(crbug.com/1403298): Replace this dependency with a delegate.
@property(nonatomic, strong) ContentSuggestionsMediator* suggestionsMediator;
// Consumer for this mediator.
@property(nonatomic, weak) id<NewTabPageConsumer> consumer;
// Consumer for NTP header model updates.
@property(nonatomic, weak) id<NewTabPageHeaderConsumer> headerConsumer;
// Delegate for controlling the current feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;
// The browser.
@property(nonatomic, assign) Browser* browser;
// The web state associated with this NTP.
@property(nonatomic, assign) web::WebState* webState;

// Inits the mediator.
- (void)setUp;

// Cleans the mediator.
- (void)shutdown;

// Save the NTP scroll offset into the last committed navigation item for the
// before navigating away.
- (void)saveContentOffsetForWebState:(web::WebState*)webState;

// Handles the actions following a tap on the "Manage Activity" item in the
// Discover feed menu.
- (void)handleFeedManageActivityTapped;

// Handles the actions following a tap on the "Manage Interests" item in the
// Discover feed menu.
- (void)handleFeedManageInterestsTapped;

// Handles the actions following a tap on the "Manage Hidden" item in the
// Discover feed menu.
- (void)handleFeedManageHiddenTapped;

// Handles the actions following a tap on the "Learn More" item in the Discover
// feed menu.
- (void)handleFeedLearnMoreTapped;

// Handles the actions following a tap on the "Visit Site" item in the followed
// item edit menu of the follow management page.
- (void)handleVisitSiteFromFollowManagementList:(const GURL&)url;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_MEDIATOR_H_
