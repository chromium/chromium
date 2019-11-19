// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLOB_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_BLOB_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/mojom/blob_storage_context.mojom.h"

namespace content {

// An IO-thread bound wrapper for the BlobStorageContext remote.
// This gets passed through many layers of classes on different sequences
// and so it's easier to have the remote bound on the correct sequence
// and share that remote than it is to bind and clone repeatedly.
// This is also more efficient than SharedRemote as we know that everything
// will use this remote from the IO thread.
//
// TODO(enne): once cache storage and idb have been converted to talk to the
// blob system over mojo, there should be no more need for the IO thread hops
// and everything could be run on the same sequence, eliminating the need
// for this class.
class CONTENT_EXPORT BlobStorageContextWrapper
    : public base::RefCountedThreadSafe<BlobStorageContextWrapper,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  // Must be called from the IO thread.
  BlobStorageContextWrapper(
      mojo::PendingRemote<storage::mojom::BlobStorageContext> context);

  // Must be called from the IO thread.
  mojo::Remote<storage::mojom::BlobStorageContext>& context();

 private:
  friend class base::RefCountedThreadSafe<BlobStorageContextWrapper>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<BlobStorageContextWrapper>;

  // Must be destroyed on the IO thread, but RefCountedThreadSafe takes
  // care of that automatically.
  ~BlobStorageContextWrapper();

  mojo::Remote<storage::mojom::BlobStorageContext> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLOB_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_
