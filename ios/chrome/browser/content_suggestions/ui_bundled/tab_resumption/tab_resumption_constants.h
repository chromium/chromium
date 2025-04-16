// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for the TabResumptionView.
extern NSString* const kTabResumptionViewIdentifier;

// Command line flag to show the item immediately without waiting for favicon.
// Mainly used in tests to avoid network requests.
extern const char kTabResumptionShowItemImmediately[];

// Accessibility ID for price tracking a URL on a recent Tab if the price
// tracking action was successful.
extern NSString* const kPriceTrackingOnTabSuccessAccessibilityID;

// Accessibility ID for price tracking a URL on a recent Tab if the price
// tracking action was not successful.
extern NSString* const kPriceTrackingOnTabFailureAccessibilityID;

// Category for price tracking the URL on a recent Tab snackbar.
extern NSString* const kPriceTrackingOnTabSnackbarCategory;

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_CONSTANTS_H_
