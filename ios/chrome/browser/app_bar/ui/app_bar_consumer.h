// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_

// Consumer of the app bar.
@protocol AppBarConsumer <NSObject>

// Updates the tab count displayed in the app bar.
- (void)updateTabCount:(NSUInteger)count;

// Called when the tab grid is about to be shown.
- (void)willEnterTabGrid;

// Called when the tab grid is about to be hidden.
- (void)willExitTabGrid;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
