// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_OPTIONS_H_
#define STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_OPTIONS_H_

#include "storage/browser/file_system/file_system_options.h"

namespace storage {

// Returns Filesystem options for incognito mode.
FileSystemOptions CreateIncognitoFileSystemOptions();

// Returns Filesystem options that allow file access.
FileSystemOptions CreateAllowFileAccessOptions();

// Returns Filesystem options that disallow file access.
FileSystemOptions CreateDisallowFileAccessOptions();

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_TEST_FILE_SYSTEM_OPTIONS_H_
