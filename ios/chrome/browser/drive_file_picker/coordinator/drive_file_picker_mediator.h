// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"

@protocol DriveFilePickerMediatorDelegate;

namespace web {
class WebState;
}

// Mediator of the Drive file picker.
@interface DriveFilePickerMediator : NSObject <DriveFilePickerMutator>

// A delegate to browse a given drive folder or search in drive.
@property(nonatomic, weak) id<DriveFilePickerMediatorDelegate> delegate;

// Initializes the mediator with a given `webState`.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects from the model layer.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
