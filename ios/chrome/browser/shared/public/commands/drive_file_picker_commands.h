// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_

#import <UIKit/UIKit.h>

@class ComposeboxPickerDriveResult;
@class ComposeboxSnackbarPresenter;
@protocol SystemIdentity;

// Commands protocol to receive Drive file picker results or cancellation.
@protocol DriveFilePickerResponseCommands <NSObject>

// Called when Drive items are selected and confirmed in the picker.
- (void)driveFilePickerDidPickItems:
    (NSArray<ComposeboxPickerDriveResult*>*)items;

// Called when the Drive file picker is dismissed or cancelled without selecting
// items.
- (void)driveFilePickerDidCancel;

@end

namespace web {
class WebState;
}

// Commands to show/hide the Drive file picker.
@protocol DriveFilePickerCommands <NSObject>

// Shows the Drive file picker for the current WebState.
// The user must have a primary identity.
- (void)showDriveFilePicker;

// Hides the Drive file picker.
- (void)hideDriveFilePicker;

// Updates the identity of the root drive folder.
// `selectedIdentity` must be non nil.
- (void)setDriveFilePickerSelectedIdentity:(id<SystemIdentity>)selectedIdentity;

// Shows the Drive file picker for the Composebox context.
- (void)
    showDriveFilePickerWithResponseHandler:
        (id<DriveFilePickerResponseCommands>)responseHandler
                        baseViewController:(UIViewController*)baseViewController
                        maxAttachmentCount:(NSUInteger)maxAttachmentCount
                         snackbarPresenter:
                             (ComposeboxSnackbarPresenter*)snackbarPresenter;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DRIVE_FILE_PICKER_COMMANDS_H_
