// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_DEVICE_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_DEVICE_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>

// Device util functions containing functions that do not require Objective-C.

// Returns the height of the screen in the current orientation.
CGFloat CurrentScreenHeight();

// Returns the width of the screen in the current orientation.
CGFloat CurrentScreenWidth();

// Returns true if the device is considered as a small device.
bool IsSmallDevice();

#endif  // IOS_CHROME_COMMON_UI_UTIL_DEVICE_UTIL_H_
