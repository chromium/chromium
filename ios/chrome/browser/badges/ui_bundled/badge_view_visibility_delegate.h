// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_VIEW_VISIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_VIEW_VISIBILITY_DELEGATE_H_

// A delegate for the badge view visibility.
@protocol BadgeViewVisibilityDelegate

// Show/hide the badge view.
- (void)setBadgeViewHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_BADGE_VIEW_VISIBILITY_DELEGATE_H_
