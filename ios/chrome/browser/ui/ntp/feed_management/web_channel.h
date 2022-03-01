// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_WEB_CHANNEL_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_WEB_CHANNEL_H_

#import <Foundation/Foundation.h>

// A view model representing a web channel.
@interface WebChannel : NSObject

// Title of the web channel.
@property(nonatomic, copy) NSString* title;

// The hostname to display.
@property(nonatomic, copy) NSString* hostname;

// YES if the web channel is unavailable.
@property(nonatomic, assign) BOOL unavailable;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_WEB_CHANNEL_H_
