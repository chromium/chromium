// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRAG_AND_DROP_DRAG_ITEM_UTIL_H_
#define IOS_CHROME_BROWSER_DRAG_AND_DROP_DRAG_ITEM_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/window_activities/window_activity_helpers.h"

class GURL;

namespace web {
class WebState;
}

// Information that allows the receiver to locate a tab and also to decide
// whether to allow a drop.
@interface TabInfo : NSObject
// The unique identifier of the tab.
@property(nonatomic, copy, readonly) NSString* tabID;
// If YES, the tab is currently in an incognito profile.
@property(nonatomic, assign, readonly) BOOL incognito;
// Default initializer.
- (instancetype)initWithTabID:(NSString*)tabID incognito:(BOOL)incognito;
- (instancetype)init NS_UNAVAILABLE;
@end

// Wrapper object that includes URL and title.
@interface URLInfo : NSObject
// The URL.
@property(nonatomic, assign, readonly) GURL URL;
// Title of the page at the URL.
@property(nonatomic, copy, readonly) NSString* title;
// Default initializer.
- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title;
- (instancetype)init NS_UNAVAILABLE;
@end

// Creates a drag item that encapsulates a tab and a user activity to move the
// tab to a new window.
UIDragItem* CreateTabDragItem(web::WebState* web_state);

// Creates a drag item that encapsulates an URL and a user activity to open the
// URL in a new Chrome window.
UIDragItem* CreateURLDragItem(URLInfo* url_info, WindowActivityOrigin origin);

#endif  // IOS_CHROME_BROWSER_DRAG_AND_DROP_DRAG_ITEM_UTIL_H_
