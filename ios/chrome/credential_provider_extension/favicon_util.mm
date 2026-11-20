// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/favicon_util.h"

#import "base/check_is_test.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {

// Maximum number of favicons kept in memory. The cache only needs to cover the
// credential rows visible at once (up to ~20) so that reloading the list
// doesn't flash the default icon. The extra margin covers scrolling back by
// about a screen, and accounts for `NSCache` not guaranteeing LRU eviction.
constexpr NSUInteger kFaviconCacheCountLimit = 40;

// Folder overriding the shared favicon folder in tests. Read on the thread
// calling `FetchFaviconAsync` (the main thread in practice) and never on the
// background queue, so that tests can reset it safely.
NSURL* g_favicon_folder_for_testing = nil;

// Returns the in-memory cache of decoded favicons, keyed by favicon file name.
// `NSCache` is thread-safe. Entries are not invalidated when Chrome refreshes
// a favicon file; this is acceptable given the short lifetime of the
// extension process.
NSCache<NSString*, FaviconAttributes*>* FaviconCache() {
  static NSCache<NSString*, FaviconAttributes*>* cache = [] {
    NSCache<NSString*, FaviconAttributes*>* new_cache = [[NSCache alloc] init];
    new_cache.countLimit = kFaviconCacheCountLimit;
    return new_cache;
  }();
  return cache;
}

// Returns the queue on which favicons are read from disk. Favicons are shown
// on screen, hence the user-initiated QoS. The queue is serial so that
// scrolling through a long list doesn't spawn one thread per blocking read.
// GCD is used because Chromium task runners are not available in the
// extension.
dispatch_queue_t FaviconQueue() {
  static dispatch_queue_t queue = dispatch_queue_create(
      "com.google.chrome.credential_provider_extension.favicon",
      dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
                                              QOS_CLASS_USER_INITIATED, 0));
  return queue;
}

// Reads and decodes the favicon attributes stored in `favicon` in `folder`.
// Returns nil on failure.
FaviconAttributes* ReadFaviconAttributes(NSURL* folder, NSString* favicon) {
  if (!folder) {
    return nil;
  }
  NSURL* file_path = [folder URLByAppendingPathComponent:favicon
                                             isDirectory:NO];
  NSError* error = nil;
  NSData* data = [NSData dataWithContentsOfURL:file_path
                                       options:0
                                         error:&error];
  if (!data || error) {
    return nil;
  }
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = YES;
  return [unarchiver decodeObjectOfClass:[FaviconAttributes class]
                                  forKey:NSKeyedArchiveRootObjectKey];
}

}  // namespace

void SetFaviconFolderForTesting(NSURL* folder) {
  CHECK_IS_TEST();
  g_favicon_folder_for_testing = folder;
  // Cached favicons belong to the previous folder. Clear them on the favicon
  // queue so that a read already queued for the previous folder can't
  // repopulate the cache afterwards.
  dispatch_sync(FaviconQueue(), ^{
    [FaviconCache() removeAllObjects];
  });
}

FaviconAttributes* CachedFaviconAttributes(NSString* favicon) {
  if (!favicon.length) {
    return nil;
  }
  return [FaviconCache() objectForKey:favicon];
}

void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion) {
  // Capture the test override before hopping to the background queue.
  NSURL* const folder_for_testing = g_favicon_folder_for_testing;
  dispatch_async(FaviconQueue(), ^{
    if (!favicon.length) {
      completion(nil);
      return;
    }
    FaviconAttributes* attributes = [FaviconCache() objectForKey:favicon];
    if (!attributes) {
      attributes = ReadFaviconAttributes(
          folder_for_testing ?: app_group::SharedFaviconAttributesFolder(),
          favicon);
      if (attributes) {
        [FaviconCache() setObject:attributes forKey:favicon];
      }
    }
    completion(attributes);
  });
}
