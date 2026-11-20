// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;

typedef void (^BlockWithFaviconAttributes)(FaviconAttributes*);

// Fetches the favicon attributes of the provided favicon and calls the
// completion block with the favicon attributes, or nil if they couldn't be
// read. The completion block is called asynchronously on a background queue;
// callers must hop to the main queue before touching UI. Successfully read
// favicons are cached in memory.
void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion);

// Returns the in-memory favicon attributes of the provided favicon if they
// were previously fetched, nil otherwise.
FaviconAttributes* CachedFaviconAttributes(NSString* favicon);

// Overrides the folder favicons are read from and clears the in-memory cache.
// Pass nil to reset. Must be called on the main thread.
void SetFaviconFolderForTesting(NSURL* folder);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
