// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_DATA_CONTROLS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_DATA_CONTROLS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test app interface for data controls policies.
@interface DataControlsAppInterface : NSObject

// Sets a data controls policy that blocks copy actions.
+ (void)setBlockCopyRule;

// Sets a data controls policy that warns on copy actions.
+ (void)setWarnCopyRule;

// Clears all data controls policies.
+ (void)clearDataControlRules;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_TEST_DATA_CONTROLS_APP_INTERFACE_H_
