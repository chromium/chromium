// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"

@implementation ComposeboxAttachmentSelection

- (instancetype)initWithTabIDs:(std::set<web::WebStateID>)tabIDs
             cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs
                        images:(NSArray<ComposeboxPickerImageResult*>*)images
                         files:(NSArray<NSURL*>*)files
                    driveItems:
                        (NSArray<ComposeboxPickerDriveResult*>*)driveItems {
  self = [super init];
  if (self) {
    _tabIDs = tabIDs;
    _cachedWebStateIDs = cachedWebStateIDs;
    _images = [images copy];
    _files = [files copy];
    _driveItems = [driveItems copy];
  }
  return self;
}

- (BOOL)hasAttachments {
  return self.images.count > 0 || self.files.count > 0 ||
         !self.tabIDs.empty() || self.driveItems.count > 0;
}

@end
