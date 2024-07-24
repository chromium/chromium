// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_

// TabStripPresentation contains methods that control how the tab strip is
// displayed on the screen.
@protocol TabStripPresentation

// Returns YES if the tab strip is fully visible. Returns NO if it is partially
// visible or not visible.
- (BOOL)isTabStripFullyVisible;

// Asks the implementer to show the given `tabStripView`.
- (void)showTabStripView:(UIView*)tabStripView;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_PRESENTATION_H_
