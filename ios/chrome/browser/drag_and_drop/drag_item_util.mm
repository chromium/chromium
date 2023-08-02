// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"

#import "base/check_op.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"

@implementation TabInfo
- (instancetype)initWithTabID:(NSString*)tabID incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _tabID = tabID;
    _incognito = incognito;
  }
  return self;
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

UIDragItem* CreateTabDragItem(web::WebState* web_state) {
  DCHECK(web_state);
  NSURL* url = net::NSURLWithGURL(web_state->GetVisibleURL());
  NSItemProvider* item_provider = [[NSItemProvider alloc] initWithObject:url];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  NSString* tab_id = web_state->GetStableIdentifier();
  BOOL incognito = web_state->GetBrowserState()->IsOffTheRecord();
  // Visibility "all" is required to allow the OS to recognize this activity for
  // creating a new window.
  [item_provider registerObject:ActivityToMoveTab(tab_id, incognito)
                     visibility:NSItemProviderRepresentationVisibilityAll];
  TabInfo* tab_info = [[TabInfo alloc] initWithTabID:tab_id
                                           incognito:incognito];
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
