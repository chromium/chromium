// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/favicon_util.h"

#import "base/check_is_test.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {

// Folder overriding the shared favicon folder in tests. Read on the thread
// calling `FetchFaviconAsync` (the main thread in practice) and never on the
// background queue, so that tests can reset it safely.
NSURL* g_favicon_folder_for_testing = nil;

}  // namespace

void SetFaviconFolderForTesting(NSURL* folder) {
  CHECK_IS_TEST();
  g_favicon_folder_for_testing = folder;
}

void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion) {
  // Capture the test override before hopping to the background queue.
  NSURL* const folder_for_testing = g_favicon_folder_for_testing;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
    NSURL* folder =
        folder_for_testing ?: app_group::SharedFaviconAttributesFolder();
    NSURL* file_path = [folder URLByAppendingPathComponent:favicon
                                               isDirectory:NO];
    NSError* error = nil;
    NSData* data = [NSData dataWithContentsOfURL:file_path
                                         options:0
                                           error:&error];
    FaviconAttributes* attributes = nil;
    if (data && !error) {
      NSKeyedUnarchiver* unarchiver =
          [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
      unarchiver.requiresSecureCoding = YES;
      attributes = [unarchiver decodeObjectOfClass:[FaviconAttributes class]
                                            forKey:NSKeyedArchiveRootObjectKey];
    }
    completion(attributes);
  });
}
