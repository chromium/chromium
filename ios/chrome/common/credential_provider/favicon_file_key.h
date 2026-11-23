// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_FAVICON_FILE_KEY_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_FAVICON_FILE_KEY_H_

#import <Foundation/Foundation.h>

// Returns whether `key` is a valid favicon file key, i.e. the hex encoded
// SHA-256 hash used as the favicon file name in the shared app group folder.
bool IsValidFaviconFileKey(NSString* key);

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_FAVICON_FILE_KEY_H_
