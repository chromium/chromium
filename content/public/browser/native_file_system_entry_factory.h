// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/native_file_system_permission_context.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Exposes methods for creating NativeFileSystemEntries. All these methods need
// to be called on the UI thread.
class CONTENT_EXPORT NativeFileSystemEntryFactory
    : public base::RefCountedThreadSafe<NativeFileSystemEntryFactory,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  using UserAction = NativeFileSystemPermissionContext::UserAction;

  // Context from which a created handle is going to be used. This is used for
  // security and permission checks. Pass in the URL most relevant as the url
  // parameter. This url will be used for verifications later for SafeBrowsing
  // and Quarantine Service if used for writes.
  struct CONTENT_EXPORT BindingContext {
    BindingContext(const url::Origin& origin,
                   const GURL& url,
                   GlobalFrameRoutingId frame_id)
        : origin(origin), url(url), frame_id(frame_id) {}
    BindingContext(const url::Origin& origin,
                   const GURL& url,
                   int worker_process_id)
        : origin(origin),
          url(url),
          frame_id(worker_process_id, MSG_ROUTING_NONE) {}
    url::Origin origin;
    GURL url;
    GlobalFrameRoutingId frame_id;
    bool is_worker() const { return !frame_id; }
    int process_id() const { return frame_id.child_id; }
  };

  enum class PathType {
    // A path on the local file system. Files with these paths can be operated
    // on by base::File.
    kLocal,

    // A path on an "external" file system. These paths can only be accessed via
    // the filesystem abstraction in //storage/browser/file_system, and a
    // storage::kFileSystemTypeExternal storage::FileSystemURL.
    // This path type should be used for paths retrieved via the `virtual_path`
    // member of a ui::SelectedFileInfo struct.
    kExternal
  };

  // Creates a new NativeFileSystemEntryPtr from the path to a file. Assumes the
  // passed in path is valid and represents a file.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& file_path,
      UserAction user_action) = 0;

  // Creates a new NativeFileSystemEntryPtr from the path to a directory.
  // Assumes the passed in path is valid and represents a directory.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      PathType path_type,
      const base::FilePath& directory_path,
      UserAction user_action) = 0;

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<NativeFileSystemEntryFactory>;
  virtual ~NativeFileSystemEntryFactory() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
