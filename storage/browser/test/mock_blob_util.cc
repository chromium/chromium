// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_blob_util.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

ScopedTextBlob::ScopedTextBlob(BlobStorageContext* context,
                               const std::string& blob_id,
                               const std::string& data)
    : blob_id_(blob_id), context_(context) {
  DCHECK(context_);
  auto blob_builder = std::make_unique<BlobDataBuilder>(blob_id_);
  if (!data.empty())
    blob_builder->AppendData(data);
  handle_ = context_->AddFinishedBlob(std::move(blob_builder));
}

ScopedTextBlob::~ScopedTextBlob() = default;

std::unique_ptr<BlobDataHandle> ScopedTextBlob::GetBlobDataHandle() {
  return context_->GetBlobDataFromUUID(blob_id_);
}

}  // namespace storage
