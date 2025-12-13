// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"

class DriveFilePickerCollection;
class DriveFilePickerImageFetcher;
@protocol DriveFilePickerMediatorDelegate;
@class DriveFilePickerMetricsHelper;
@protocol DriveFilePickerConsumer;
@protocol DriveFilePickerCommands;
struct DriveFilePickerOptions;
@protocol SystemIdentity;

namespace drive {
class DriveService;
}

namespace signin {
class IdentityManager;
}

namespace web {
class WebState;
}

class ChromeAccountManagerService;

// Mediator of the Drive file picker.
@interface DriveFilePickerMediator : NSObject <DriveFilePickerMutator>

// Dependencies.
@property(nonatomic, weak) id<DriveFilePickerMediatorDelegate> delegate;
@property(nonatomic, weak) id<DriveFilePickerConsumer> consumer;
@property(nonatomic, weak) id<DriveFilePickerCommands> driveFilePickerHandler;
@property(nonatomic, assign) raw_ptr<drive::DriveService> driveService;
@property(nonatomic, assign) raw_ptr<signin::IdentityManager> identityManager;
@property(nonatomic, assign) raw_ptr<ChromeAccountManagerService>
    accountManagerService;
@property(nonatomic, assign) raw_ptr<DriveFilePickerImageFetcher> imageFetcher;
@property(nonatomic, weak) DriveFilePickerMetricsHelper* metricsHelper;

// Whether the mediator is active (a.k.a the associated consumer is at the top
// of the navigation stack and should be updated accordingly).
@property(nonatomic, assign, getter=isActive) BOOL active;

// Pending options. This will take effect when the mediator is set to active.
@property(nonatomic, assign) std::optional<DriveFilePickerOptions>
    pendingOptions;

// Initializes the mediator with a given `webState`.
- (instancetype)initWithWebState:(web::WebState*)webState
                      collection:
                          (std::unique_ptr<DriveFilePickerCollection>)collection
                         options:(DriveFilePickerOptions)options
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects from the model layer. It is an error for the mediator to be
// deallocated without first calling `-disconnect`.
- (void)disconnect;

// Sets the collection browsed by the mediator to `collection`.
- (void)setCollection:(std::unique_ptr<DriveFilePickerCollection>)collection;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
