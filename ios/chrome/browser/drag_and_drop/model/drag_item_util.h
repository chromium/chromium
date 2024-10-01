// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_DRAG_ITEM_UTIL_H_
#define IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_DRAG_ITEM_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

class GURL;
class TabGroup;

namespace web {
class WebState;
class WebStateID;
}  // namespace web

// Information that allows the receiver to locate a tab and also to decide
// whether to allow a drop.
@interface TabInfo : NSObject
// The unique identifier of the tab.
@property(nonatomic, assign, readonly) web::WebStateID tabID;
// If YES, the tab is currently in an incognito profile.
@property(nonatomic, assign, readonly) BOOL incognito;
// A pointer to the `profile`.
@property(nonatomic, readonly) ProfileIOS* profile;

// Default initializer.
- (instancetype)initWithTabID:(web::WebStateID)tabID
                      profile:(ProfileIOS*)profile;
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

// Information that allows the receiver to locate a tab group and also to decide
// whether to allow a drop.
@interface TabGroupInfo : NSObject
// A pointer to the `tabGroup`.
@property(nonatomic, readonly) const TabGroup* tabGroup;
// If YES, the tab group is currently in an incognito profile.
@property(nonatomic, assign, readonly) BOOL incognito;
// A pointer to the `profile`.
@property(nonatomic, readonly) ProfileIOS* profile;

// Default initializer.
- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup
                         profile:(ProfileIOS*)profile;
- (instancetype)init NS_UNAVAILABLE;
@end

// Creates a drag item that encapsulates a tab and a user activity to move the
// tab to a new window. If `web_state` is nil, returns nil.
UIDragItem* CreateTabDragItem(web::WebState* web_state);

// Creates a drag item that encapsulates an URL and a user activity to open the
// URL in a new Chrome window.
UIDragItem* CreateURLDragItem(URLInfo* url_info, WindowActivityOrigin origin);

// Creates a drag item that encapsulates a tab group. The created drag item can
// only be dropped in Chrome windows.
UIDragItem* CreateTabGroupDragItem(const TabGroup* tab_group,
                                   ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_DRAG_ITEM_UTIL_H_
