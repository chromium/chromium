// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/base/clipboard/file_info.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class FileSystemContext;
class FileSystemURL;
class QuotaManagerProxy;
}  // namespace storage

namespace content {

class BrowserContext;
class ChildProcessSecurityPolicyImpl;
struct DropData;

// Helper method that returns FileSystemContext constructed for
// the browser process.
CONTENT_EXPORT scoped_refptr<storage::FileSystemContext>
CreateFileSystemContext(
    BrowserContext* browser_context,
    const base::FilePath& profile_path,
    bool is_incognito,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

// Verifies that `url` is valid and has a registered backend in `context`.
CONTENT_EXPORT bool FileSystemURLIsValid(storage::FileSystemContext* context,
                                         const storage::FileSystemURL& url);

// TODO(crbug.com/40810215): Consider making this a method on FileSystemContext.
// Get the platform path from a file system URL. This needs to be called
// on the FILE thread.
using DoGetPlatformPathCB = base::OnceCallback<void(const base::FilePath&)>;
CONTENT_EXPORT void DoGetPlatformPath(
    scoped_refptr<storage::FileSystemContext> context,
    int process_id,
    const GURL& path,
    const blink::StorageKey& storage_key,
    DoGetPlatformPathCB callback);

// Make it possible for a `drop_data`'s resources to be read by `child_id`'s
// process -- by granting permissions, rewriting `drop_data`, or both.
//
// `drop_data` can include references to local files and filesystem files that
// were accessible to the child process that is the source of the drag and drop,
// but might not (yet) be accessible to the child process that is the target of
// the drop.  PrepareDropDataForChildProcess makes sure that `child_id` has
// access to files referred to by `drop_data` - this method will 1) mutate
// `drop_data` as needed (e.g. to refer to files in a new isolated filesystem,
// rather than the original filesystem files) and 2) use `security_policy` to
// grant `child_id` appropriate file access.
CONTENT_EXPORT void PrepareDropDataForChildProcess(
    DropData* drop_data,
    ChildProcessSecurityPolicyImpl* security_policy,
    int child_id,
    const storage::FileSystemContext* file_system_context);

// Make it possible for local files to be read by `child_id`'s process. This is
// used by clipboard, and by drag-and-drop. Returns filesystem_id of the
// registered isolated filesystem.
CONTENT_EXPORT std::string PrepareDataTransferFilenamesForChildProcess(
    std::vector<ui::FileInfo>& filenames,
    ChildProcessSecurityPolicyImpl* security_policy,
    int child_id,
    const storage::FileSystemContext* file_system_context);

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_HELPER_H_
