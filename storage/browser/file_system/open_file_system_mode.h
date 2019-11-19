// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_OPEN_FILE_SYSTEM_MODE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_OPEN_FILE_SYSTEM_MODE_H_

namespace storage {

// Determines the behavior on OpenFileSystem when a specified
// FileSystem does not exist.
// Specifying CREATE_IF_NONEXISTENT may make actual modification on
// disk (e.g. creating a root directory, setting up a metadata database etc)
// if the filesystem hasn't been initialized.
enum OpenFileSystemMode {
  OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
  OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_OPEN_FILE_SYSTEM_MODE_H_
