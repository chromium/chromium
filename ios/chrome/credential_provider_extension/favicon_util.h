// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;

typedef void (^BlockWithFaviconAttributes)(FaviconAttributes*);

// Fetches the favicon attributes of `favicon` and invokes `completion`
// asynchronously on the main queue with the attributes, or nil if `favicon`
// isn't a valid favicon file key or reading or decoding failed. Must be called
// on the main thread, and `completion` must be non-nil. Successfully read
// favicons are cached in memory.
void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion);

// Returns the in-memory favicon attributes of `favicon` if they were
// previously fetched, nil otherwise.
FaviconAttributes* CachedFaviconAttributes(NSString* favicon);

// Overrides the folder favicons are read from and clears the in-memory cache.
// Pass nil to reset. Must be called on the main thread.
void SetFaviconFolderForTesting(NSURL* folder);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
