// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/features.h"

#include "base/feature_list.h"

namespace content::features {

// TODO(crbug.com/1370433): Remove this flag eventually.
// When enabled, drag-and-dropped files and directories will be checked against
// the File System Access blocklist. This feature was disabled since it broke
// some applications.
BASE_FEATURE(kFileSystemAccessDragAndDropCheckBlocklist,
             "FileSystemAccessDragAndDropCheckBlocklist",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content::features
