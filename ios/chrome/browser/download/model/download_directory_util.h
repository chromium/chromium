// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_DIRECTORY_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_DIRECTORY_UTIL_H_

#import <Foundation/Foundation.h>

namespace base {
class FilePath;
}

// Fills `directory_path` with the FilePath to the temporary downloads
// directory. Returns true if this is successful. This method does not create
// the directory, it just returns the path.
bool GetTempDownloadsDirectory(base::FilePath* directory_path);

// Fills `directory_path` with the FilePath to the downloads directory. This
// method does not create the directory, it just updates the path.
void GetDownloadsDirectory(base::FilePath* directory_path);

// Converts an absolute file path to a relative path based on the downloads
// directory.
base::FilePath ConvertToRelativeDownloadPath(
    const base::FilePath& absolute_path);

// Converts a relative file path to an absolute path by prepending the downloads
// directory.
base::FilePath ConvertToAbsoluteDownloadPath(
    const base::FilePath& relative_path);

// Asynchronously deletes downloads directory.
void DeleteTempDownloadsDirectory();

namespace test {
// Sets an override for GetDownloadsDirectory for testing purposes.
// Pass nullptr to reset to default behavior.
void SetDownloadsDirectoryForTesting(const base::FilePath* directory);
}  // namespace test

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_DIRECTORY_UTIL_H_
