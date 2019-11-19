// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_data_handle.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace storage {

BlobDataHandle::BlobDataHandleShared::BlobDataHandleShared(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    uint64_t size,
    BlobStorageContext* context)
    : uuid_(uuid),
      content_type_(content_type),
      content_disposition_(content_disposition),
      size_(size),
      context_(context->AsWeakPtr()) {
  context_->IncrementBlobRefCount(uuid);
}

std::unique_ptr<BlobReader> BlobDataHandle::CreateReader() const {
  return base::WrapUnique(new BlobReader(this));
}

BlobDataHandle::BlobDataHandleShared::~BlobDataHandleShared() {
  if (context_.get())
    context_->DecrementBlobRefCount(uuid_);
}

BlobDataHandle::BlobDataHandle(const std::string& uuid,
                               const std::string& content_type,
                               const std::string& content_disposition,
                               uint64_t size,
                               BlobStorageContext* context,
                               base::SequencedTaskRunner* io_task_runner)
    : io_task_runner_(io_task_runner),
      shared_(new BlobDataHandleShared(uuid,
                                       content_type,
                                       content_disposition,
                                       size,
                                       context)) {
  DCHECK(io_task_runner_.get());
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
}

BlobDataHandle::BlobDataHandle(const BlobDataHandle& other) = default;

BlobDataHandle::~BlobDataHandle() {
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    io_task_runner_->ReleaseSoon(FROM_HERE, std::move(shared_));
  }
}

BlobDataHandle& BlobDataHandle::operator=(
    const BlobDataHandle& other) = default;

bool BlobDataHandle::IsBeingBuilt() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_)
    return false;
  return BlobStatusIsPending(GetBlobStatus());
}

bool BlobDataHandle::IsBroken() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_)
    return true;
  return BlobStatusIsError(GetBlobStatus());
}

BlobStatus BlobDataHandle::GetBlobStatus() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_)
    return BlobStatus::ERR_REFERENCED_BLOB_BROKEN;
  return shared_->context_->GetBlobStatus(shared_->uuid_);
}

void BlobDataHandle::RunOnConstructionComplete(BlobStatusCallback done) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_.get()) {
    std::move(done).Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
    return;
  }
  shared_->context_->RunOnConstructionComplete(shared_->uuid_, std::move(done));
}

void BlobDataHandle::RunOnConstructionBegin(BlobStatusCallback done) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_.get()) {
    std::move(done).Run(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS);
    return;
  }
  shared_->context_->RunOnConstructionBegin(shared_->uuid_, std::move(done));
}

std::unique_ptr<BlobDataSnapshot> BlobDataHandle::CreateSnapshot() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!shared_->context_.get())
    return nullptr;
  return shared_->context_->CreateSnapshot(shared_->uuid_);
}

const std::string& BlobDataHandle::uuid() const {
  return shared_->uuid_;
}

const std::string& BlobDataHandle::content_type() const {
  return shared_->content_type_;
}

const std::string& BlobDataHandle::content_disposition() const {
  return shared_->content_disposition_;
}

uint64_t BlobDataHandle::size() const {
  return shared_->size_;
}

}  // namespace storage
