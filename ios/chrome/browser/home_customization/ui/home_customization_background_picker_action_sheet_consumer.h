// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol HomeCustomizationBackgroundPickerPresentationDelegate;

// A consumer protocol for the background picker's action sheet.
@protocol HomeCustomizationBackgroundPickerActionSheetConsumer

// Returns the UINavigationItem associated with the action sheet.
@property(nonatomic, strong, readonly) UINavigationItem* navigationItem;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PICKER_ACTION_SHEET_CONSUMER_H_
