// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONTROLLER_DELEGATE_H_

// Delete for NTP and it's subclasses to communicate with the toolbar.
@protocol NewTabPageControllerDelegate
// Sets the toolbar location bar alpha and vertical offset based on `progress`.
- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress;

// The target for scribble events as forwarded by the NTP fakebox.
- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget;

// NTP became active on the active web state.
- (void)didNavigateToNTPOnActiveWebState;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONTROLLER_DELEGATE_H_
