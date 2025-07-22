// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_

#import <Foundation/Foundation.h>

// Returns true if the directory already exists or when the creation is donne
// successfully
bool CreateShareExtensionFilesDirectory(NSURL* folderURL);

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_UTILS_H_
