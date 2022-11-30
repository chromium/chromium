// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_

#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// A refcounted wrapper for the BlobStorageContext remote.
// Everything in this wrapper must be called from the same sequence.
// This gets passed through many layers of classes on different sequences
// and so it's easier to have the remote bound on the correct sequence
// and share that remote than it is to bind and clone repeatedly.
class CONTENT_EXPORT BlobStorageContextWrapper
    : public base::RefCounted<BlobStorageContextWrapper> {
 public:
  BlobStorageContextWrapper(
      mojo::PendingRemote<storage::mojom::BlobStorageContext> context);

  mojo::Remote<storage::mojom::BlobStorageContext>& context();

 private:
  friend class base::RefCounted<BlobStorageContextWrapper>;

  SEQUENCE_CHECKER(sequence_checker_);

  ~BlobStorageContextWrapper();

  mojo::Remote<storage::mojom::BlobStorageContext> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_BLOB_STORAGE_CONTEXT_WRAPPER_H_
