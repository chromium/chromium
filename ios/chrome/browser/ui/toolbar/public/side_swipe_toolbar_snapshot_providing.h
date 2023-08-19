// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_SNAPSHOT_PROVIDING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_SNAPSHOT_PROVIDING_H_

#import <UIKit/UIKit.h>

enum class ToolbarType;

namespace web {
class WebState;
}  // namespace web

// Protocol used by SideSwipe to get snapshot of the toolbar.
@protocol SideSwipeToolbarSnapshotProviding

// Returns a snapshot of the toolbar with the controls visibility adapted to
// `webState`.
- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState
                                withToolbarType:(ToolbarType)toolbarType;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_SIDE_SWIPE_TOOLBAR_SNAPSHOT_PROVIDING_H_
