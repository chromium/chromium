// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSUMER_H_

// Consumes swipe navigation events.
@protocol SideSwipeConsumer

// Enables or disables navigation via swipes initiated from the leading edge
// (e.g., left edge in left-to-right languages).
- (void)setLeadingEdgeNavigationEnabled:(BOOL)enabled;

// Enables or disables navigation via swipes initiated from the trailing edge
// (e.g., right edge in left-to-right languages).
- (void)setTrailingEdgeNavigationEnabled:(BOOL)enabled;

// Cancels any ongoing side swipe gesture.
- (void)cancelOnGoingSwipe;

// Notifies the consumer that the web page has finished loading.
- (void)webPageLoaded;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_CONSUMER_H_
