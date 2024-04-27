// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content::features {

// When enabled, pages in the BFCache can be evicted when they hold
// FileSystemAccessLockManager Locks that are contentious with the Locks of an
// active page. blink::features::kFileSystemAccessLockingScheme must be enabled
// as well for this to have any effect.
BASE_FEATURE(kFileSystemAccessBFCache,
             "FileSystemAccessBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/40061211): Remove this flag eventually.
// When enabled, drag-and-dropped files and directories will be checked against
// the File System Access blocklist. This feature was disabled since it broke
// some applications.
BASE_FEATURE(kFileSystemAccessDragAndDropCheckBlocklist,
             "FileSystemAccessDragAndDropCheckBlocklist",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40896420): Remove this flag eventually.
// When enabled, GetFile() and GetEntries() on a directory handle performs
// the blocklist check on child file handles.
BASE_FEATURE(kFileSystemAccessDirectoryIterationBlocklistCheck,
             "FileSystemAccessDirectoryIterationBlocklistCheck",
#if BUILDFLAG(IS_WIN)
// On Windows, resolving a symlink by getting an absolute path does not work,
// and it requires a different implementation approach. Enable it on Windows
// once available.
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
// TODO(crbug.com/328115444): Temporarily disabled due to the bug returning
// different results.
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content::features
