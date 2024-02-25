// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_

#include "base/files/file_path.h"

namespace signin {
enum class Tribool;
}  // namespace signin

// File name for sentinel to backup in iOS backup device.
extern const base::FilePath::CharType kSentinelThatIsBackedUp[];
// File name for sentinel to not backup in iOS backup device.
extern const base::FilePath::CharType kSentinelThatIsNotBackedUp[];

base::FilePath PathForSentinel(const base::FilePath::CharType* sentinel_name);

// Returns whether Chrome has been started after a device restore. This method
// needs to be called for the first time before IO is disallowed on UI thread.
// The value is cached. The result is cached for later calls.
signin::Tribool IsFirstSessionAfterDeviceRestoreInternal();

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_UTIL_INTERNAL_H_
