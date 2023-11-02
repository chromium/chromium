// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content::features {

// TODO(crbug.com/1370433): Remove this flag eventually.
// When enabled, drag-and-dropped files and directories will be checked against
// the File System Access blocklist. This feature was disabled since it broke
// some applications.
BASE_FEATURE(kFileSystemAccessDragAndDropCheckBlocklist,
             "FileSystemAccessDragAndDropCheckBlocklist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1114923): Remove this flag eventually.
// When enabled, the remove() method is enabled. Otherwise, throws a
// NotSupportedError DomException.
BASE_FEATURE(kFileSystemAccessRemove,
             "FileSystemAccessRemove",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1247850): Remove this flag eventually.
// When enabled, move operations within the same file system that do not change
// the file extension will not be subject to safe browsing checks.
BASE_FEATURE(kFileSystemAccessSkipAfterWriteChecksIfUnchangingExtension,
             "FileSystemAccessSkipAfterWriteChecksIfUnchangingExtension",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1421735): Remove this flag eventually.
// When enabled, GetFile() and GetEntries() on the directory handle resolve
// symbolic link (if any) and check the path against the blocklis, on POSIX.
// This feature was disabled since it broke some applications.
BASE_FEATURE(kFileSystemAccessDirectoryIterationSymbolicLinkCheck,
             "FileSystemAccessDirectoryIterationSymbolicLinkCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content::features
