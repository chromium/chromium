// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_NSCODER_COMPATIBILITY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_NSCODER_COMPATIBILITY_H_

#import <Foundation/Foundation.h>

@interface NSCoder (Compatibility)

// Decodes and returns a 32-bit or 64-bit integer value that was previously
// encoded with `cr_encodeIndex:forKey:` and associated with `key`. Handles
// with special care `NSNotFound` value to be portable between 32-bit and
// 64-bit binaries.
- (NSInteger)cr_decodeIndexForKey:(NSString*)key;

// Encodes the 32-bit or 64-bit integer `index` and associates it with `key`.
// Handles with special care `NSNotFound` value to be portable between 32-bit
// and 64-bit binaries.
- (void)cr_encodeIndex:(NSInteger)index forKey:(NSString*)key;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_NSCODER_COMPATIBILITY_H_
