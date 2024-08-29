// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_file_data.h"

@implementation ShareFileData

- (instancetype)initWithFilePath:(NSURL*)filePath {
  if ((self = [super init])) {
    _filePath = filePath;
  }
  return self;
}

@end
