// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_HANDLE_H_
#define STORAGE_BROWSER_BLOB_BLOB_HANDLE_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

// Refcounted wrapper around a mojom::Blob.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobHandle
    : public base::RefCounted<BlobHandle> {
 public:
  explicit BlobHandle(mojo::PendingRemote<blink::mojom::Blob> blob);

  bool is_bound() const { return blob_.is_bound(); }
  blink::mojom::Blob* get() const { return blob_.get(); }
  blink::mojom::Blob* operator->() const { return get(); }
  blink::mojom::Blob& operator*() const { return *get(); }

  mojo::PendingRemote<blink::mojom::Blob> Clone() const;

 private:
  friend class base::RefCounted<BlobHandle>;
  ~BlobHandle();

  mojo::Remote<blink::mojom::Blob> blob_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_HANDLE_H_
