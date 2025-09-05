// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_SNACKBAR_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_SNACKBAR_UTIL_H_

#import <UIKit/UIKit.h>

@class SnackbarMessage;

// Returns a SnackbarMessage instance. During EGTests, the snackbar duration
// is updated to have the maximum duration.
SnackbarMessage* CreateCustomSnackbarMessage(NSString* text);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_SNACKBAR_UTIL_H_
