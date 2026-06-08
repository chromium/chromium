// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_DRIVE_RESULT_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_DRIVE_RESULT_H_

#import <Foundation/Foundation.h>

// Represents a picked Google Drive file result.
@interface ComposeboxPickerDriveResult : NSObject

/// The unique identifier of the Drive item.
@property(nonatomic, copy) NSString* identifier;
/// The file name of the Drive item.
@property(nonatomic, copy) NSString* fileName;
/// The mime type of the Drive item.
@property(nonatomic, copy) NSString* mimeType;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_PICKER_DRIVE_RESULT_H_
