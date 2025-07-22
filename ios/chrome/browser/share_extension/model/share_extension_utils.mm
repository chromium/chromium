// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_utils.h"

#import "base/threading/scoped_blocking_call.h"

bool CreateShareExtensionFilesDirectory(NSURL* folderURL) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* manager = [NSFileManager defaultManager];
  NSError* error = nil;
  if ([manager fileExistsAtPath:[folderURL path]]) {
    return true;
  }

  bool shareExtensionFilesDirectoryCreated =
      [manager createDirectoryAtPath:[folderURL path]
          withIntermediateDirectories:NO
                           attributes:nil
                                error:&error];
  if (error) {
    return false;
  }

  return shareExtensionFilesDirectoryCreated;
}
