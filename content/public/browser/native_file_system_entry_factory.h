// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Exposes methods for creating NativeFileSystemEntries. All these methods need
// to be called on the UI thread.
class CONTENT_EXPORT NativeFileSystemEntryFactory
    : public base::RefCountedThreadSafe<NativeFileSystemEntryFactory,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  // Context from which a created handle is going to be used. This is used for
  // security and permission checks. Pass in MSG_ROUTING_NONE as frame_id if
  // the context is a worker, otherwise use the routing id of the relevant
  // RenderFrameHost. Pass in the URL most relevant as the url parameter.
  // This url will be used for verifications later for SafeBrowsing and
  // Quarantine Service if used for writes.
  struct CONTENT_EXPORT BindingContext {
    BindingContext(const url::Origin& origin,
                   const GURL& url,
                   int process_id,
                   int frame_id)
        : origin(origin),
          url(url),
          process_id(process_id),
          frame_id(frame_id) {}
    url::Origin origin;
    GURL url;
    int process_id;
    int frame_id;
    bool is_worker() const { return frame_id == MSG_ROUTING_NONE; }
  };

  // Creates a new NativeFileSystemEntryPtr from the path to a file. Assumes the
  // passed in path is valid and represents a file.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& file_path) = 0;

  // Creates a new NativeFileSystemEntryPtr from the path to a directory.
  // Assumes the passed in path is valid and represents a directory.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& directory_path) = 0;

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<NativeFileSystemEntryFactory>;
  virtual ~NativeFileSystemEntryFactory() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
