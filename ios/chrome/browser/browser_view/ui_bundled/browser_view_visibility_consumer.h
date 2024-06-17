// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_VISIBILITY_CONSUMER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_VISIBILITY_CONSUMER_H_

/// Consumer protocol that gets notified when the browser view's visibility has
/// changed.
@protocol BrowserViewVisibilityConsumer

/// Method that responds to browser view visibility changes.
- (void)browserViewDidChangeVisibility;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_VISIBILITY_CONSUMER_H_
