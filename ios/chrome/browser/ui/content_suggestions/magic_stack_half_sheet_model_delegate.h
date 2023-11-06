// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MODEL_DELEGATE_H_

#import <UIKit/UIKit.h>

// Model delegate for the Magic Stack Half Sheet.
@protocol MagicStackHalfSheetModelDelegate

// Indicate to the model delegate that the Set Up List enabled state changed.
- (void)setUpListEnabledChanged:(BOOL)setUpListEnabled;

// Indicate to the model delegate that the Safety Check enabled state changed.
- (void)safetyCheckEnabledChanged:(BOOL)safetyCheckEnabled;

// Indicate to the model delegate that the Tab Resumption enabled state changed.
- (void)tabResumptionEnabledChanged:(BOOL)tabResumptionEnabled;

// Indicates to the model delegate that the Parcel Tracking enabled state
// changed.
- (void)parcelTrackingEnabledChanged:(BOOL)parcelTrackingEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_HALF_SHEET_MODEL_DELEGATE_H_
