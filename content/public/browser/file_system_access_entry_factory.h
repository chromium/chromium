// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_ENTRY_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_ENTRY_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/global_routing_id.h"
#include "ipc/ipc_message.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"
#include "url/gurl.h"

namespace content {

// Exposes methods for creating FileSystemAccessEntries. All these methods need
// to be called on the UI thread.
class CONTENT_EXPORT FileSystemAccessEntryFactory
    : public base::RefCountedThreadSafe<FileSystemAccessEntryFactory,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  using UserAction = FileSystemAccessPermissionContext::UserAction;

  // Context from which a created handle is going to be used. This is used for
  // security and permission checks. Pass in the URL most relevant as the url
  // parameter. This url will be used for verifications later for SafeBrowsing
  // and Quarantine Service if used for writes.
  struct CONTENT_EXPORT BindingContext {
    // Used for frames that have an associated rfh (e.g. Dedicated Workers,
    // Render Frame Host).
    BindingContext(const blink::StorageKey& storage_key,
                   const GURL& url,
                   GlobalRenderFrameHostId frame_id,
                   bool is_worker = false)
        : storage_key(storage_key),
          url(url),
          frame_id(frame_id),
          is_worker(is_worker) {}
    // Used for frames that don't have an associated rfh (e.g. Service Workers,
    // Shared Workers).
    BindingContext(const blink::StorageKey& storage_key,
                   const GURL& url,
                   int worker_process_id)
        : storage_key(storage_key),
          url(url),
          frame_id(worker_process_id, MSG_ROUTING_NONE),
          is_worker(true) {}
    blink::StorageKey storage_key;
    GURL url;
    GlobalRenderFrameHostId frame_id;
    bool is_worker;
    int process_id() const { return frame_id.child_id; }
  };

  // Creates a new FileSystemAccessEntryPtr from the path to a file. Assumes the
  // passed in path is valid and represents a file.
  virtual blink::mojom::FileSystemAccessEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const content::PathInfo& path_info,
      UserAction user_action) = 0;

  // Creates a new FileSystemAccessEntryPtr from the path to a directory.
  // Assumes the passed in path is valid and represents a directory.
  virtual blink::mojom::FileSystemAccessEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const content::PathInfo& path_info,
      UserAction user_action) = 0;

  // Resolve a FileSystemAccessTransferToken to its FileSystemURL. Invokes the
  // callback with a std::nullopt if the token isn't valid or can't be found
  // (e.g. a compromised renderer crafts an invalid token).
  using ResolveTransferTokenCallback =
      base::OnceCallback<void(std::optional<storage::FileSystemURL>)>;
  virtual void ResolveTransferToken(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
          transfer_token,
      ResolveTransferTokenCallback callback) = 0;

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<FileSystemAccessEntryFactory>;
  virtual ~FileSystemAccessEntryFactory() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_ENTRY_FACTORY_H_
