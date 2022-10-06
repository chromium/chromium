// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/data/share_file_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShareFileData

- (instancetype)initWithFilePath:(NSURL*)filePath {
  if (self = [super init]) {
    _filePath = filePath;
  }
  return self;
}

@end
