// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_VIEW_DELEGATE_H_

class GURL;
@class TabView;

// Protocol to observe events occuring in the tab view.
@protocol TabViewDelegate
// Called when the TabView was tapped.
- (void)tabViewTapped:(TabView*)tabView;

// Called when the TabView's close button was tapped.
- (void)tabViewcloseButtonPressed:(TabView*)tabView;

// Called when an item that can be interpreted as a URL is dropped on the tab
// view.
- (void)tabView:(TabView*)tabView receivedDroppedURL:(GURL)url;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_VIEW_DELEGATE_H_
