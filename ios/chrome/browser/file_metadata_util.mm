// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/file_metadata_util.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void SetSkipSystemBackupAttributeToItem(const base::FilePath& path,
                                        bool skip_system_backup) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  NSURL* file_url =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  DCHECK([[NSFileManager defaultManager] fileExistsAtPath:file_url.path]);

  NSError* error = nil;
  BOOL success = [file_url setResourceValue:(skip_system_backup ? @YES : @NO)
                                     forKey:NSURLIsExcludedFromBackupKey
                                      error:&error];
  if (!success) {
    LOG(ERROR) << base::SysNSStringToUTF8([error description]);
  }
}
