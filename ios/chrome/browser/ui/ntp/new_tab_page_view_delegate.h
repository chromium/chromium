// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_DELEGATE_H_

// Delegate for NTP view information to be passed to the Content Suggestions.
@protocol NewTabPageViewDelegate

// Returns the necessary padding between the Home modules and the sides of the
// screen. This can range anywhere between 0 and `HomeModuleMinimumPadding()`,
// depending on the screen size.
- (CGFloat)homeModulePadding;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_DELEGATE_H_
