// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_CONSUMER_H_

#import "ios/chrome/browser/save_to_drive/ui_bundled/file_destination.h"

@protocol FileDestinationPickerConsumer

- (void)setSelectedDestination:(FileDestination)destination;

@end

#endif  // IOS_CHROME_BROWSER_SAVE_TO_DRIVE_UI_BUNDLED_FILE_DESTINATION_PICKER_CONSUMER_H_
