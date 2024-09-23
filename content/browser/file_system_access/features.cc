// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content::features {

// When enabled, pages in the BFCache can be evicted when they hold
// FileSystemAccessLockManager Locks that are contentious with the Locks of an
// active page.
BASE_FEATURE(kFileSystemAccessBFCache,
             "FileSystemAccessBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/40061211): Remove this flag eventually.
//
// When enabled, drag-and-dropped directories will be checked against the File
// System Access blocklist. This feature was disabled since it broke some
// applications.
BASE_FEATURE(kFileSystemAccessDragAndDropCheckBlocklist,
             "FileSystemAccessDragAndDropCheckBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/40896420): Remove this flag eventually.
// TODO(b/354661640): Temporarily disable this flag while investigating CrOS
// file saving issue.
//
// When enabled, GetFile() and GetEntries() on a directory handle performs
// the blocklist check on child file handles.
BASE_FEATURE(kFileSystemAccessDirectoryIterationBlocklistCheck,
             "FileSystemAccessDirectoryIterationBlocklistCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace content::features
