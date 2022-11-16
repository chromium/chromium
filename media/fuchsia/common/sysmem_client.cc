// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_client.h"

#include <lib/sys/cpp/component_context.h>
#include <zircon/rights.h>

#include <algorithm>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/process/process_handle.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

SysmemCollectionClient::SysmemCollectionClient(
    fuchsia::sysmem::Allocator* allocator,
    fuchsia::sysmem::BufferCollectionTokenPtr collection_token)
    : allocator_(allocator), collection_token_(std::move(collection_token)) {
  DCHECK(allocator_);
  DCHECK(collection_token_);
}

SysmemCollectionClient::~SysmemCollectionClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (collection_)
    collection_->Close();
}

void SysmemCollectionClient::Initialize(
    fuchsia::sysmem::BufferCollectionConstraints constraints,
    base::StringPiece name,
    uint32_t name_priority) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  writable_ = (constraints.usage.cpu & fuchsia::sysmem::cpuUsageWrite) ==
              fuchsia::sysmem::cpuUsageWrite;

  allocator_->BindSharedCollection(std::move(collection_token_),
                                   collection_.NewRequest());

  collection_.set_error_handler(
      fit::bind_member(this, &SysmemCollectionClient::OnError));
  collection_->SetName(name_priority, std::string(name));

  // If Sync() is not required then constraints can be set immediately.
  if (sync_completion_closures_.empty()) {
    collection_->SetConstraints(/*has_constraints=*/true,
                                std::move(constraints));
    return;
  }

  sync_completion_closures_.push_back(
      base::BindOnce(&fuchsia::sysmem::BufferCollection::SetConstraints,
                     base::Unretained(collection_.get()),
                     /*have_constraints=*/true, std::move(constraints)));
  collection_->Sync(
      fit::bind_member(this, &SysmemCollectionClient::OnSyncComplete));
}

void SysmemCollectionClient::CreateSharedToken(
    GetSharedTokenCB cb,
    base::StringPiece debug_client_name,
    uint64_t debug_client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(collection_token_);

  fuchsia::sysmem::BufferCollectionTokenPtr token;
  collection_token_->Duplicate(ZX_RIGHT_SAME_RIGHTS, token.NewRequest());

  if (!debug_client_name.empty()) {
    token->SetDebugClientInfo(std::string(debug_client_name), debug_client_id);
  }

  sync_completion_closures_.push_back(
      base::BindOnce(std::move(cb), std::move(token)));
}

void SysmemCollectionClient::AcquireBuffers(AcquireBuffersCB cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!collection_token_);

  if (!collection_) {
    std::move(cb).Run({}, {});
    return;
  }

  acquire_buffers_cb_ = std::move(cb);
  collection_->WaitForBuffersAllocated(
      fit::bind_member(this, &SysmemCollectionClient::OnBuffersAllocated));
}

void SysmemCollectionClient::OnSyncComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<base::OnceClosure> sync_closures =
      std::move(sync_completion_closures_);
  for (auto& cb : sync_closures) {
    std::move(cb).Run();
  }
}

void SysmemCollectionClient::OnBuffersAllocated(
    zx_status_t status,
    fuchsia::sysmem::BufferCollectionInfo_2 buffer_collection_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "Failed to allocate sysmem buffers.";
    OnError(status);
    return;
  }

  if (acquire_buffers_cb_) {
    auto buffers = VmoBuffer::CreateBuffersFromSysmemCollection(
        &buffer_collection_info, writable_);
    std::move(acquire_buffers_cb_)
        .Run(std::move(buffers), buffer_collection_info.settings);
  }
}

void SysmemCollectionClient::OnError(zx_status_t status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ZX_DLOG(ERROR, status) << "Connection to BufferCollection was disconnected.";
  collection_.Unbind();
  if (acquire_buffers_cb_)
    std::move(acquire_buffers_cb_).Run({}, {});
}

SysmemAllocatorClient::SysmemAllocatorClient(base::StringPiece client_name) {
  allocator_ = base::ComponentContextForProcess()
                   ->svc()
                   ->Connect<fuchsia::sysmem::Allocator>();

  allocator_->SetDebugClientInfo(std::string(client_name),
                                 base::GetCurrentProcId());

  allocator_.set_error_handler([](zx_status_t status) {
    // Just log a warning. We will handle BufferCollection the failure when
    // trying to create a new BufferCollection.
    ZX_DLOG(WARNING, status)
        << "The fuchsia.sysmem.Allocator channel was disconnected.";
  });
}

SysmemAllocatorClient::~SysmemAllocatorClient() = default;

fuchsia::sysmem::BufferCollectionTokenPtr
SysmemAllocatorClient::CreateNewToken() {
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token;
  allocator_->AllocateSharedCollection(collection_token.NewRequest());
  return collection_token;
}

std::unique_ptr<SysmemCollectionClient>
SysmemAllocatorClient::AllocateNewCollection() {
  return std::make_unique<SysmemCollectionClient>(allocator_.get(),
                                                  CreateNewToken());
}

std::unique_ptr<SysmemCollectionClient>
SysmemAllocatorClient::BindSharedCollection(
    fuchsia::sysmem::BufferCollectionTokenPtr token) {
  return std::make_unique<SysmemCollectionClient>(allocator_.get(),
                                                  std::move(token));
}

}  // namespace media
