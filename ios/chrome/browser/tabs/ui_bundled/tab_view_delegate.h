// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_DELEGATE_H_

class GURL;
@class TabView;

// Protocol to observe events occuring in the tab view.
@protocol TabViewDelegate
// Called when the TabView was tapped.
- (void)tabViewTapped:(TabView*)tabView;

// Called when the TabView's close button was tapped.
- (void)tabViewCloseButtonPressed:(TabView*)tabView;

// Called when an item that can be interpreted as a URL is dropped on the tab
// view.
- (void)tabView:(TabView*)tabView receivedDroppedURL:(GURL)url;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_VIEW_DELEGATE_H_
