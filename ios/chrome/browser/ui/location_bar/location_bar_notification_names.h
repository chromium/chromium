// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_NOTIFICATION_NAMES_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_NOTIFICATION_NAMES_H_

#import <Foundation/Foundation.h>

// Notification sent when the location bar becomes first responder. In UI
// Refresh, it is not repeatedly sent if the omnibox is refocused while the
// popup is still open.
extern NSString* const kLocationBarBecomesFirstResponderNotification;
// Notification sent when the location bar resigns first responder. Not sent
// when the omnibox resigns first responder while the popup is still open (i.e.
// when the popup is scrolled).
extern NSString* const kLocationBarResignsFirstResponderNotification;

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_NOTIFICATION_NAMES_H_
