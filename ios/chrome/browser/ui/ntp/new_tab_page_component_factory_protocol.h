// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_

class Browser;
namespace web {
class WebState;
}

@class ContentSuggestionsCoordinator;
@class ContentSuggestionsHeaderViewController;
@class FeedMetricsRecorder;
@class NewTabPageMediator;
@class NewTabPageViewController;
@protocol UserAccountImageUpdateDelegate;

@protocol NewTabPageComponentFactoryProtocol

// Coordinator for the ContentSuggestions.
- (ContentSuggestionsCoordinator*)contentSuggestionsCoordinatorForBrowser:
    (Browser*)browser;

// Metrics recorder for actions relating to the feed.
- (FeedMetricsRecorder*)feedMetricsRecorderForBrowser:(Browser*)browser;

// The header view controller containing the fake omnibox and logo.
- (ContentSuggestionsHeaderViewController*)headerController;

// Mediator owned by the NewTabPageCoordinator
- (NewTabPageMediator*)NTPMediatorForBrowser:(Browser*)browser
                                    webState:(web::WebState*)webState
                    identityDiscImageUpdater:
                        (id<UserAccountImageUpdateDelegate>)imageUpdater;

// View controller for the regular NTP.
- (NewTabPageViewController*)NTPViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_PROTOCOL_H_
