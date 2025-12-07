// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_FILE_SIZE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_FILE_SIZE_UTIL_H_

#import <Foundation/Foundation.h>

#import <cstdint>

// Returns a human-readable string representation of a file size in bytes.
// Uses NSByteCountFormatter with file count style and replaces spaces with
// non-breaking spaces for consistent display.
NSString* GetSizeString(int64_t size_in_bytes);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_FILE_SIZE_UTIL_H_
