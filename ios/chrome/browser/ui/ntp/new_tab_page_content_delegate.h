// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_

// Delegate for actions relating to the NTP content.
@protocol NewTabPageContentDelegate

// Reloads content suggestions collection view.
- (void)reloadContentSuggestions;

// YES if the content requires the header to stick while scrolling.
- (BOOL)isContentHeaderSticky;

// Handles what happens when the signin promo changes visibility in the NTP.
- (void)signinPromoHasChangedVisibility:(BOOL)visible;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONTENT_DELEGATE_H_
