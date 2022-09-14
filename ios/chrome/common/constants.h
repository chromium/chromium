// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CONSTANTS_H_
#define IOS_CHROME_COMMON_CONSTANTS_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Time to expire passwords copied to the pasteboard.
extern const NSTimeInterval kSecurePasteboardExpiration;

// The em-width value used to differentiate small and large devices.
// With Larger Text Off, Bold Text Off and the device orientation in portrait:
// iPhone 5s is considered as a small device, unlike iPhone 8 or iPhone 12 mini.
extern const CGFloat kSmallDeviceThreshold;

#endif  // IOS_CHROME_COMMON_CONSTANTS_H_
