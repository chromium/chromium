// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_context.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace content {

class BrowserContext;
class ChildProcessSecurityPolicyImpl;
struct DropData;

// Helper method that returns FileSystemContext constructed for
// the browser process.
CONTENT_EXPORT scoped_refptr<storage::FileSystemContext>
CreateFileSystemContext(BrowserContext* browser_context,
                        const base::FilePath& profile_path,
                        bool is_incognito,
                        storage::QuotaManagerProxy* quota_manager_proxy);

// Verifies that |url| is valid and has a registered backend in |context|.
CONTENT_EXPORT bool FileSystemURLIsValid(storage::FileSystemContext* context,
                                         const storage::FileSystemURL& url);

// Get the platform path from a file system URL. This needs to be called
// on the FILE thread.
using SyncGetPlatformPathCB = base::OnceCallback<void(const base::FilePath&)>;
CONTENT_EXPORT void SyncGetPlatformPath(storage::FileSystemContext* context,
                                        int process_id,
                                        const GURL& path,
                                        SyncGetPlatformPathCB callback);

// Make it possible for a |drop_data|'s resources to be read by |child_id|'s
// process -- by granting permissions, rewriting |drop_data|, or both.
//
// |drop_data| can include references to local files and filesystem files that
// were accessible to the child process that is the source of the drag and drop,
// but might not (yet) be accessible to the child process that is the target of
// the drop.  PrepareDropDataForChildProcess makes sure that |child_id| has
// access to files referred to by |drop_data| - this method will 1) mutate
// |drop_data| as needed (e.g. to refer to files in a new isolated filesystem,
// rather than the original filesystem files) and 2) use |security_policy| to
// grant |child_id| appropriate file access.
CONTENT_EXPORT void PrepareDropDataForChildProcess(
    DropData* drop_data,
    ChildProcessSecurityPolicyImpl* security_policy,
    int child_id,
    const storage::FileSystemContext* file_system_context);

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
