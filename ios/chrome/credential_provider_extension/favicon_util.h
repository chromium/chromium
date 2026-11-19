// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;

typedef void (^BlockWithFaviconAttributes)(FaviconAttributes*);

// Fetches the favicon attributes of the provided favicon and calls the
// completion block with the favicon attributes. The completion block is called
// on a background queue; callers must hop to the main queue before touching UI.
void FetchFaviconAsync(NSString* favicon,
                       BlockWithFaviconAttributes completion);

// Overrides the folder favicons are read from. Pass nil to reset. Must be
// called on the main thread.
void SetFaviconFolderForTesting(NSURL* folder);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_FAVICON_UTIL_H_
