// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"

#import <utility>

@implementation ComposeboxAttachmentSelection

- (instancetype)
    initWithTabSelection:(ComposeboxTabSelection)tabSelection
                  images:(NSArray<ComposeboxPickerImageResult*>*)images
                   files:(NSArray<NSURL*>*)files
              driveItems:(NSArray<ComposeboxPickerDriveResult*>*)driveItems {
  self = [super init];
  if (self) {
    _tabSelection = std::move(tabSelection);
    _images = [images copy];
    _files = [files copy];
    _driveItems = [driveItems copy];
  }
  return self;
}

- (BOOL)hasAttachments {
  return self.images.count > 0 || self.files.count > 0 ||
         !_tabSelection.selected_ids.empty() || self.driveItems.count > 0;
}

@end
