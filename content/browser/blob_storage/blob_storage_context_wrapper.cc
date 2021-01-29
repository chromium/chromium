// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_storage_context_wrapper.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"

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
