// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"

#import "base/check_op.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

@implementation TabInfo {
  // Weak reference of the chrome browser state.
  base::WeakPtr<ChromeBrowserState> _weakBrowserState;
}

- (instancetype)initWithTabID:(web::WebStateID)tabID
                 browserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    CHECK(tabID.valid());
    _tabID = tabID;
    _weakBrowserState = browserState->AsWeakPtr();
    _incognito = browserState->IsOffTheRecord();
  }
  return self;
}

#pragma mark - Getters

- (ChromeBrowserState*)browserState {
  return _weakBrowserState.get();
}

@end

@implementation URLInfo
- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title {
  self = [super init];
  if (self) {
    _URL = URL;
    _title = title;
  }
  return self;
}
@end

@implementation TabGroupInfo {
  // Weak reference of the dragged tab group.
  base::WeakPtr<const TabGroup> _weakTabGroup;
  // Weak reference of the chrome browser state.
  base::WeakPtr<ChromeBrowserState> _weakBrowserState;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
                    browserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _weakTabGroup = tabGroup->GetWeakPtr();
    _weakBrowserState = browserState->AsWeakPtr();
    _incognito = browserState->IsOffTheRecord();
  }
  return self;
}

#pragma mark - Getters

- (const TabGroup*)tabGroup {
  return _weakTabGroup.get();
}

- (ChromeBrowserState*)browserState {
  return _weakBrowserState.get();
}

@end

UIDragItem* CreateTabDragItem(web::WebState* web_state) {
  if (!web_state) {
    return nil;
  }
  NSURL* url = net::NSURLWithGURL(web_state->GetVisibleURL());
  NSItemProvider* item_provider = [[NSItemProvider alloc] initWithObject:url];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  web::WebStateID tab_id = web_state->GetUniqueIdentifier();
  ChromeBrowserState* browserState =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  BOOL incognito = browserState->IsOffTheRecord();
  // Visibility "all" is required to allow the OS to recognize this activity for
  // creating a new window.
  [item_provider registerObject:ActivityToMoveTab(tab_id, incognito)
                     visibility:NSItemProviderRepresentationVisibilityAll];
  TabInfo* tab_info = [[TabInfo alloc] initWithTabID:tab_id
                                        browserState:browserState];
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  drag_item.localObject = tab_info;
  return drag_item;
}

UIDragItem* CreateURLDragItem(URLInfo* url_info, WindowActivityOrigin origin) {
  DCHECK(url_info.URL.is_valid());
  NSItemProvider* item_provider =
      [[NSItemProvider alloc] initWithObject:net::NSURLWithGURL(url_info.URL)];
  // Visibility "all" is required to allow the OS to recognize this activity for
  // creating a new window.
  [item_provider registerObject:ActivityToLoadURL(origin, url_info.URL)
                     visibility:NSItemProviderRepresentationVisibilityAll];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  drag_item.localObject = url_info;
  return drag_item;
}

UIDragItem* CreateTabGroupDragItem(const TabGroup* tab_group,
                                   ChromeBrowserState* browser_state) {
  if (!tab_group) {
    return nil;
  }

  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:[[NSItemProvider alloc] init]];
  TabGroupInfo* tab_group_info =
      [[TabGroupInfo alloc] initWithTabGroup:tab_group
                                browserState:browser_state];
  drag_item.localObject = tab_group_info;
  return drag_item;
}
