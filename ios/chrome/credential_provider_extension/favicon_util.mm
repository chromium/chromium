// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/favicon_util.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion) {
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
    NSURL* filePath = [app_group::SharedFaviconAttributesFolder()
        URLByAppendingPathComponent:favicon
                        isDirectory:NO];
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfURL:filePath
                                         options:0
                                           error:&error];
    FaviconAttributes* attributes = nil;
    if (data && !error) {
      NSKeyedUnarchiver* unarchiver =
          [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
      unarchiver.requiresSecureCoding = NO;
      attributes = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    }
    completion(attributes);
  });
}
