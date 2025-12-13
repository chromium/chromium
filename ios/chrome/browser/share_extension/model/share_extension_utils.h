// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_

#import <Foundation/Foundation.h>

@class ParsedShareExtensionEntry;

// Returns true if the directory already exists or when the creation is donne
// successfully
bool CreateShareExtensionFilesDirectory(NSURL* folder_url);

// Returns an array containing the files in a given directory.
NSArray<NSURL*>* EnumerateFilesInDirectory(NSURL* directory_url);

// Reads and parses a given file and returns a parsed share extension entry.
ParsedShareExtensionEntry* PerformBlockingFileReadAndParse(NSURL* file_url);

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_
