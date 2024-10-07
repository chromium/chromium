// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_mutator.h"

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

class ShareKitService;
@protocol TabGroupIndicatorConsumer;
@protocol TabGroupIndicatorMediatorDelegate;
class UrlLoadingBrowserAgent;
class WebStateList;

// Mediator used to propagate tab group updates to the TabGroupIndicatorView.
@interface TabGroupIndicatorMediator : NSObject <TabGroupIndicatorMutator>

// Delegate for actions happening in the mediator.
@property(nonatomic, weak) id<TabGroupIndicatorMediatorDelegate> delegate;

// The view controller on which to present the share view.
@property(nonatomic, strong) UIViewController* baseViewController;

// Creates an instance of the mediator.
- (instancetype)initWithTabGroupSyncService:
                    (tab_groups::TabGroupSyncService*)tabGroupSyncService
                            shareKitService:(ShareKitService*)shareKitService
                                   consumer:
                                       (id<TabGroupIndicatorConsumer>)consumer
                               webStateList:(WebStateList*)webStateList
                                  URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                                  incognito:(BOOL)incognito;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_MEDIATOR_H_
