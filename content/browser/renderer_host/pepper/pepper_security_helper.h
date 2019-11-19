// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

// Helper function that returns whether or not the child process is allowed to
// open the specified |file| with the specified |pp_open_flags|.
CONTENT_EXPORT bool CanOpenWithPepperFlags(int pp_open_flags,
                                           int child_id,
                                           const base::FilePath& file);

// Helper function that returns whether or not the child process is allowed to
// open the specified file system |url| with the specified |pp_open_flags|.
CONTENT_EXPORT bool CanOpenFileSystemURLWithPepperFlags(
    int pp_open_flags,
    int child_id,
    const storage::FileSystemURL& url);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SECURITY_HELPER_H_
