// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Accessibility identifier for the snackbar view.
extern NSString* const kSnackbarAccessibilityId;
// Accessibility identifier for the snackbar title label.
extern NSString* const kSnackbarTitleAccessibilityId;
// Accessibility identifier for the snackbar subtitle label.
extern NSString* const kSnackbarSubtitleAccessibilityId;
// Accessibility identifier for the snackbar secondary subtitle label.
extern NSString* const kSnackbarSecondarySubtitleAccessibilityId;
// Accessibility identifier for the snackbar button.
extern NSString* const kSnackbarButtonAccessibilityId;
// Accessibility identifier for the snackbar leading accessory view.
extern NSString* const kSnackbarLeadingAccessoryAccessibilityId;
// Accessibility identifier for the snackbar trailing accessory view.
extern NSString* const kSnackbarTrailingAccessoryAccessibilityId;

// Duration for snackbars.
inline constexpr base::TimeDelta kSnackbarMessageDuration = base::Seconds(4);

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_SNACKBAR_SNACKBAR_CONSTANTS_H_
