// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/blob_storage_context_wrapper.h"

namespace content {

BlobStorageContextWrapper::BlobStorageContextWrapper(
    mojo::PendingRemote<storage::mojom::BlobStorageContext> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  context_.Bind(std::move(context));
}

mojo::Remote<storage::mojom::BlobStorageContext>&
BlobStorageContextWrapper::context() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_;
}

BlobStorageContextWrapper::~BlobStorageContextWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace content
