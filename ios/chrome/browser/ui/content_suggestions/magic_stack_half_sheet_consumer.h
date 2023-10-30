// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer of the MagicStackHalfSheetMediator.
@protocol MagicStackHalfSheetConsumer

// Indicates to the consumer whether to `showSetUpList`.
- (void)showSetUpList:(BOOL)showSetUpList;

// Indicates to the consumer that `setUpListDisabled`.
- (void)setSetUpListDisabled:(BOOL)setUpListDisabled;

// Indicates to the consumer that `safetyCheckDisabled`.
- (void)setSafetyCheckDisabled:(BOOL)safetyCheckDisabled;

// Indicates to the consumer that `tabResumptionDisabled`.
- (void)setTabResumptionDisabled:(BOOL)tabResumptionDisabled;

// Indicates to the consumer that `parcelTrackingDisabled`.
- (void)setParcelTrackingDisabled:(BOOL)parcelTrackingDisabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_CONSUMER_H_
