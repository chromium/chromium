// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

extern const int kInfobarBackgroundColor;

// a11y identifier so that automation can tap on either infobar button
extern NSString* const kConfirmInfobarButton1AccessibilityIdentifier;
extern NSString* const kConfirmInfobarButton2AccessibilityIdentifier;

// The duration in seconds that the InfobarCoordinator banner will be presented
// for.
extern const NSTimeInterval kInfobarBannerDefaultPresentationDurationInSeconds;
// The duration in seconds that a high priority presentation InfobarCoordinator
// banner will be presented for.
extern const NSTimeInterval kInfobarBannerLongPresentationDurationInSeconds;

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONSTANTS_H_
