// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_PREVIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_PREVIEW_DELEGATE_H_

#import <UIKit/UIKit.h>

class GURL;

// Protocol for actions relating to the Discover feed preview.
@protocol DiscoverFeedPreviewDelegate

// A view controller which displays a preview of `URL`.
- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL;

// Handles the action when users tap on the discover feed preview.
- (void)didTapDiscoverFeedPreview;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_PREVIEW_DELEGATE_H_
