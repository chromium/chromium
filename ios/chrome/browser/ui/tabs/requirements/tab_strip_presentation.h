// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_
#define IOS_CHROME_BROWSER_UI_TABS_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_

@protocol TabStripContaining;

// TabStripPresentation contains methods that control how the tab strip is
// displayed on the screen.
@protocol TabStripPresentation

// Returns YES if the tab strip is fully visible. Returns NO if it is partially
// visible or not visible.
- (BOOL)isTabStripFullyVisible;

// Asks the implementer to show the given `tabStripView`.
- (void)showTabStripView:(UIView<TabStripContaining>*)tabStripView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_
