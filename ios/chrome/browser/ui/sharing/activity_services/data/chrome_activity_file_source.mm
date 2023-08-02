// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_file_source.h"

#import "base/check.h"

@implementation ChromeActivityFileSource {
  // Path where the downloaded file is saved.
  NSURL* _filePath;
}

- (instancetype)initWithFilePath:(NSURL*)filePath {
  DCHECK(filePath);
  self = [super init];
  if (self) {
    _filePath = filePath;
  }
  return self;
}

#pragma mark - ChromeActivityItemSource

- (NSSet*)excludedActivityTypes {
  return [NSSet setWithArray:@[ UIActivityTypeCopyToPasteboard ]];
}

#pragma mark - UIActivityItemSource

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return _filePath;
}

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return _filePath;
}

@end
