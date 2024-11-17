// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PATHS_PATHS_INTERNAL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PATHS_PATHS_INTERNAL_H_

namespace base {
class FilePath;
}

namespace ios {

// Get the path to the user's cache directory. Note that Chrome on iOS cache
// directories are actually subdirectories of this directory with names like
// "Cache". This will always fill in `result` with a directory, sometimes
// just `profile_dir`.
void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result);

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PATHS_PATHS_INTERNAL_H_
