// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_handle.h"

namespace storage {

BlobHandle::BlobHandle(mojo::PendingRemote<blink::mojom::Blob> blob)
    : blob_(std::move(blob)) {
  DCHECK(blob_);
}

mojo::PendingRemote<blink::mojom::Blob> BlobHandle::Clone() const {
  mojo::PendingRemote<blink::mojom::Blob> clone;
  blob_->Clone(clone.InitWithNewPipeAndPassReceiver());
  return clone;
}

BlobHandle::~BlobHandle() = default;

}  // namespace storage
