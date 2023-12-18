// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"

@implementation SaveToDriveCoordinator {
  NSString* _fileName;
  int64_t _fileSize;
  // TODO(crbug.com/1495352): Add an account picker coordinator to let the user
  // select the identity with which they wish to save the file to Drive.
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  fileName:(NSString*)fileName
                                  fileSize:(int64_t)fileSize {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _fileName = fileName;
    _fileSize = fileSize;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // TODO(crbug.com/1495352): Start the account picker coordinator.
}

- (void)stop {
  // TODO(crbug.com/1495352): Stop the account picker coordinator.
}

@end
