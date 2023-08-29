// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_LOGGER_H_
#define IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_LOGGER_H_

#import <Foundation/Foundation.h>

// Protocol for features presenting the account picker to log events.
@protocol AccountPickerLogger <NSObject>

// Called when the account picker list is opened.
- (void)logAccountPickerSelectionScreenOpened;

// Called when the account picker list is closed.
- (void)logAccountPickerSelectionScreenClosed;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_LOGGER_H_
