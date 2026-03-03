// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_COLOR_UPDATER_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_COLOR_UPDATER_H_

// Protocol for the FacePileView to update colors.
@protocol FacePileColorUpdater <NSObject>

// Sets the background color for the shareButton.
- (void)setShareButtonBackgroundColor:(UIColor*)backgroundColor;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_UI_FACE_PILE_COLOR_UPDATER_H_
