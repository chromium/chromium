// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol BrowseDriveFilePickerCoordinatorDelegate;
@protocol SystemIdentity;

namespace web {
class WebState;
}

// Coordinator of the Browse Drive file picker.
@interface BrowseDriveFilePickerCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<BrowseDriveFilePickerCoordinatorDelegate>
    delegate;

// Creates a coordinator that uses `viewController`, `browser`, `webState` and
// `folder`.
- (instancetype)
    initWithBaseNavigationViewController:
        (UINavigationController*)baseNavigationController
                                 browser:(Browser*)browser
                                webState:(base::WeakPtr<web::WebState>)webState
                                   title:(NSString*)title
                           imagesPending:(NSMutableSet<NSString*>*)imagesPending
                              imageCache:
                                  (NSCache<NSString*, UIImage*>*)imageCache
                          collectionType:
                              (DriveFilePickerCollectionType)collectionType
                        folderIdentifier:(NSString*)folderIdentifier
                                  filter:(DriveFilePickerFilter)filter
                     ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
                         sortingCriteria:(DriveItemsSortingType)sortingCriteria
                        sortingDirection:
                            (DriveItemsSortingOrder)sortingDirection
                                identity:(id<SystemIdentity>)identity
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_BROWSE_DRIVE_FILE_PICKER_COORDINATOR_H_
