// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_commands.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

@class ContentSuggestionsCoordinator;
@class ContentSuggestionsHeaderViewController;
@class NewTabPageViewController;

// This is a private category that is intended to only be imported in
// new_tab_page_coordinator.mm and tests.
@interface NewTabPageCoordinator (Private) <ContentSuggestionsHeaderCommands,
                                            SceneStateObserver,
                                            WebStateListObserving>

@property(nonatomic, strong, readonly)
    ContentSuggestionsHeaderViewController* headerController;

@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

@property(nonatomic, assign) web::WebState* webState;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
@property(nonatomic, readonly) BOOL visible;

@property(nonatomic, strong) NewTabPageViewController* NTPViewController;

- (void)configureNTPViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_PRIVATE_H_
