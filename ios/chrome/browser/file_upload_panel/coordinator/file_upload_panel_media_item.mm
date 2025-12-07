// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/coordinator/file_upload_panel_media_item.h"

@implementation FileUploadPanelMediaItem

- (instancetype)initWithFileURL:(NSURL*)fileURL isVideo:(BOOL)isVideo {
  self = [super init];
  if (self) {
    self.fileURL = fileURL;
    self.isVideo = isVideo;
  }
  return self;
}

@end
