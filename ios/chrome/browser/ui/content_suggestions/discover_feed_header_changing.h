// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_HEADER_CHANGING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_HEADER_CHANGING_H_

// Protocol for handling UI changes for the Discover feed header.
@protocol DiscoverFeedHeaderChanging

// Handles Discover header UI for when feed visibility preference is changed.
- (void)changeDiscoverFeedHeaderVisibility:(BOOL)visible;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_HEADER_CHANGING_H_
