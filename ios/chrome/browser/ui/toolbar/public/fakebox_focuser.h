// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FAKEBOX_FOCUSER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FAKEBOX_FOCUSER_H_

#import <Foundation/Foundation.h>

// This protocol provides callbacks for focusing and blurring the fake omnibox
// on NTP.
@protocol FakeboxFocuser
// Focuses the omnibox without animations.
- (void)focusOmniboxNoAnimation;
// Give focus to the omnibox, but indicate that the focus event was initiated
// from the fakebox on the Google landing page.
- (void)focusOmniboxFromFakebox:(BOOL)fromFakebox pinned:(BOOL)pinned;
// Hides the toolbar when the fakebox is blurred.
- (void)onFakeboxBlur;
// Shows the toolbar when the fakebox has animated to full bleed.
- (void)onFakeboxAnimationComplete;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FAKEBOX_FOCUSER_H_
