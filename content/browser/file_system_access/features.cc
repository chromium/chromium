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

// TODO(crbug.com/1381621): Remove this flag eventually.
// When enabled, move() will result in a promise rejection when the specified
// destination to move to exists. This feature was disabled since it does not
// match standard POSIX behavior. See discussion at
// https://github.com/whatwg/fs/pull/10#issuecomment-1322993643.
BASE_FEATURE(kFileSystemAccessDoNotOverwriteOnMove,
             "FileSystemAccessDoNotOverwriteOnMove",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1140805): Remove this flag eventually.
// When enabled, move() supports moving local files (i.e. that do not live in
// the OPFS).
BASE_FEATURE(kFileSystemAccessMoveLocalFiles,
             "FileSystemAccessMoveLocalFiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1114923): Remove this flag eventually.
// When enabled, the remove() method is enabled. Otherwise, throws a
// NotSupportedError DomException.
BASE_FEATURE(kFileSystemAccessRemove,
             "FileSystemAccessRemove",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/1394837): Remove this flag eventually.
// When enabled, a user gesture is required to rename a file if the site does
// not have write access to the parent. See http://b/254157070 for more context.
BASE_FEATURE(kFileSystemAccessRenameWithoutParentAccessRequiresUserActivation,
             "FileSystemAccessRenameWithoutParentAccessRequiresUserActivation",
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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1413443): Remove this flag eventually.
// When enabled, createWritable({ keepExistingData:true }) will create a swap
// file using APFS's built-in support for copy-on-write files instead of copying
// over the file's contents manually.
BASE_FEATURE(kFileSystemAccessCowSwapFile,
             "FileSystemAccessCowSwapFile",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace content::features
