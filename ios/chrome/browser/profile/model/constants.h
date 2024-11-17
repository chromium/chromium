// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_CONSTANTS_H_

#include "base/files/file_path.h"

// A handful of resource-like constants related to the Chrome application.

extern const char kIOSChromeInitialProfile[];

extern const base::FilePath::CharType kIOSChromeCacheDirname[];
extern const base::FilePath::CharType kIOSChromeCookieFilename[];
extern const base::FilePath::CharType
    kIOSChromeNetworkPersistentStateFilename[];

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_CONSTANTS_H_
