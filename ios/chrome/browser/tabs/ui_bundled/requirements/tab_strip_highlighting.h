// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_HIGHLIGHTING_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_HIGHLIGHTING_H_

// Protocol to enable highlighting of the tab strip.
@protocol TabStripHighlighting
// When toggled on, this animates the tab strip by dimming all non-selected
// tabs. When toggled off, this animates the tab strip to normal.
- (void)setHighlightsSelectedTab:(BOOL)highlightsSelectedTab;
@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_REQUIREMENTS_TAB_STRIP_HIGHLIGHTING_H_
