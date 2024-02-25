// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_CONSUMER_H_

#import "ios/chrome/browser/ui/save_to_drive/file_destination.h"

@protocol FileDestinationPickerConsumer

- (void)setSelectedDestination:(FileDestination)destination;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_FILE_DESTINATION_PICKER_CONSUMER_H_
