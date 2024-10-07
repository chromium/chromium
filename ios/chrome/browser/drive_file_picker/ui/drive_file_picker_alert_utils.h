// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ALERT_UTILS_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ALERT_UTILS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Returns an alert informing the user of a failed download.
UIAlertController* FailAlertController(ProceduralBlock retry_block,
                                       ProceduralBlock cancel_block);

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_UI_DRIVE_FILE_PICKER_ALERT_UTILS_H_
