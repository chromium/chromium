// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSTANTS_H_

#import <Foundation/Foundation.h>

// A11y identifiers for switch to toggle camera permissions in infobar modal.
extern NSString* const kInfobarModalCameraSwitchAccessibilityIdentifier;

// A11y identifiers for switch to toggle microphone permissions in infobar
// modal.
extern NSString* const kInfobarModalMicrophoneSwitchAccessibilityIdentifier;

// A11y identifiers for switch to toggle camera permissions in infobar modal.
extern NSString* const kPageInfoCameraSwitchAccessibilityIdentifier;

// A11y identifiers for switch to toggle microphone permissions in infobar
// modal.
extern NSString* const kPageInfoMicrophoneSwitchAccessibilityIdentifier;

// Histogram name for when a permission is changed within Page Info used in all
// platforms.
extern const char kOriginInfoPermissionChangedHistogram[];

// Histogram name for when a permission is blocked within Page Info used in all
// platforms.
extern const char kOriginInfoPermissionChangedBlockedHistogram[];

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_UI_BUNDLED_PERMISSIONS_CONSTANTS_H_
