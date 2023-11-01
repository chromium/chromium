// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

// Default referer used by Discover Feed navigations.
extern const char kDefaultDiscoverReferrer[];

// The feature parameter to specify the referrer for Discover Feed navigations.
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverReferrerParameter[];

// The max width of the feed content. Currently hard coded in Mulder.
// TODO(crbug.com/1085419): Get card width from Mulder.
extern const CGFloat kDiscoverFeedContentMaxWidth;

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_CONSTANTS_H_
