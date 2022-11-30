// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/tabs/tab_strip_containing.h"

@class TabStripView;

// Container for the Tab Strip view, allowing to snapshot it.
@interface TabStripContainerView : UIView <TabStripContaining>

// A weak reference to the Tab Strip view.
@property(nonatomic, weak) TabStripView* tabStripView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTAINER_VIEW_H_
