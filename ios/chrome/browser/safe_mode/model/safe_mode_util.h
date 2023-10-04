// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_UTIL_H_
#define IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_UTIL_H_

#include <string>
#include <vector>

namespace safe_mode_util {

// Returns a list of the paths of all images (e.g., dynamic libraries)
// currently loaded.
// If `path_filter` is non-NULL, only paths starting with `path_filter` will be
// returned.
std::vector<std::string> GetLoadedImages(const char* path_filter);

}  // namespace safe_mode_util

#endif  // IOS_CHROME_BROWSER_SAFE_MODE_MODEL_SAFE_MODE_UTIL_H_
