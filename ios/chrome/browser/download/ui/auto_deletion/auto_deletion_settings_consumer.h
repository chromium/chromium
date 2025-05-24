// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_CONSUMER_H_

// Protocol for the Download Auto-deletion settings menu UI.
@protocol AutoDeletionSettingsConsumer <NSObject>

// Updates the value of the toggle switch that represents whether the feature is
// enabled/disabled.
- (void)setAutoDeletionEnabled:(BOOL)status;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_CONSUMER_H_
