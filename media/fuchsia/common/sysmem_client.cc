// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/common/sysmem_client.h"

#include <lib/sys/cpp/component_context.h>
#include <zircon/rights.h>

#include <algorithm>
#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

SysmemCollectionClient::SysmemCollectionClient(
    fuchsia::sysmem2::Allocator* allocator,
    fuchsia::sysmem2::BufferCollectionTokenPtr collection_token)
    : allocator_(allocator), collection_token_(std::move(collection_token)) {
  DCHECK(allocator_);
  DCHECK(collection_token_);
}

SysmemCollectionClient::~SysmemCollectionClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (collection_)
    collection_->Release();
}

void SysmemCollectionClient::Initialize(
    fuchsia::sysmem2::BufferCollectionConstraints constraints,
    std::string_view name,
    uint32_t name_priority) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t cpu = 0;
  if (constraints.has_usage() && constraints.usage().has_cpu()) {
    cpu = constraints.usage().cpu();
  }
  writable_ = (cpu & fuchsia::sysmem2::CPU_USAGE_WRITE) ==
              fuchsia::sysmem2::CPU_USAGE_WRITE;

  allocator_->BindSharedCollection(
      std::move(fuchsia::sysmem2::AllocatorBindSharedCollectionRequest{}
                    .set_token(std::move(collection_token_))
                    .set_buffer_collection_request(collection_.NewRequest())));

  collection_.set_error_handler(
      fit::bind_member(this, &SysmemCollectionClient::OnError));
  collection_->SetName(std::move(fuchsia::sysmem2::NodeSetNameRequest{}
                                     .set_priority(name_priority)
                                     .set_name(std::string(name))));

  // We may need to send a Sync to ensure previously-started CreateSharedToken()
  // calls can complete. The Sync completion is how we know that sysmem knows
  // about the existence of the tokens created by the CreateSharedToken() calls,
  // which is needed before we can send the token to a different participant.
  //
  // CreateSharedToken can complete as soon as this Sync is done.
  if (!shared_token_ready_closures_.empty()) {
    collection_->Sync(
        fit::bind_member(this, &SysmemCollectionClient::OnSyncComplete));
  }

  collection_->SetConstraints(std::move(
      fuchsia::sysmem2::BufferCollectionSetConstraintsRequest{}.set_constraints(
          std::move(constraints))));
}

void SysmemCollectionClient::CreateSharedToken(
    GetSharedTokenCB cb,
    std::string_view debug_client_name,
    uint64_t debug_client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(collection_token_);

  fuchsia::sysmem2::BufferCollectionTokenPtr token;
  collection_token_->Duplicate(
      std::move(fuchsia::sysmem2::BufferCollectionTokenDuplicateRequest{}
                    .set_rights_attenuation_mask(ZX_RIGHT_SAME_RIGHTS)
                    .set_token_request(token.NewRequest())));

  if (!debug_client_name.empty()) {
    token->SetDebugClientInfo(
        std::move(fuchsia::sysmem2::NodeSetDebugClientInfoRequest{}
                      .set_name(std::string(debug_client_name))
                      .set_id(debug_client_id)));
  }

  shared_token_ready_closures_.push_back(
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
  collection_->WaitForAllBuffersAllocated(
      fit::bind_member(this, &SysmemCollectionClient::OnBuffersAllocated));
}

void SysmemCollectionClient::OnSyncComplete(
    fuchsia::sysmem2::Node_Sync_Result sync_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<base::OnceClosure> sync_closures =
      std::move(shared_token_ready_closures_);
  for (auto& cb : sync_closures) {
    std::move(cb).Run();
  }
}

void SysmemCollectionClient::OnBuffersAllocated(
    fuchsia::sysmem2::BufferCollection_WaitForAllBuffersAllocated_Result
        wait_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!wait_result.is_response()) {
    zx_status_t error_status;
    if (wait_result.is_framework_err()) {
      ZX_LOG(ERROR, fidl::ToUnderlying(wait_result.framework_err()))
          << "Failed to allocate sysmem buffers (framework_err).";
      error_status = ZX_ERR_INTERNAL;
    } else {
      ZX_LOG(ERROR, static_cast<uint32_t>(wait_result.err()))
          << "Failed to allocate sysmem buffers (err).";
      // no real need to translate from sysmem2::Error to zx_status_t here
      error_status = ZX_ERR_INTERNAL;
    }
    OnError(error_status);
    return;
  }
  auto buffer_collection_info =
      std::move(*wait_result.response().mutable_buffer_collection_info());

  if (acquire_buffers_cb_) {
    auto buffers = VmoBuffer::CreateBuffersFromSysmemCollection(
        &buffer_collection_info, writable_);
    std::move(acquire_buffers_cb_)
        .Run(std::move(buffers), buffer_collection_info.settings());
  }
}

void SysmemCollectionClient::OnError(zx_status_t status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ZX_DLOG(ERROR, status) << "Connection to BufferCollection was disconnected.";
  collection_.Unbind();
  if (acquire_buffers_cb_)
    std::move(acquire_buffers_cb_).Run({}, {});
}

SysmemAllocatorClient::SysmemAllocatorClient(std::string_view client_name) {
  allocator_ = base::ComponentContextForProcess()
                   ->svc()
                   ->Connect<fuchsia::sysmem2::Allocator>();

  allocator_->SetDebugClientInfo(
      std::move(fuchsia::sysmem2::AllocatorSetDebugClientInfoRequest{}
                    .set_name(std::string(client_name))
                    .set_id(base::GetCurrentProcId())));

  allocator_.set_error_handler([](zx_status_t status) {
    // Just log a warning. We will handle BufferCollection the failure when
    // trying to create a new BufferCollection.
    ZX_DLOG(WARNING, status)
        << "The fuchsia.sysmem.Allocator channel was disconnected.";
  });
}

SysmemAllocatorClient::~SysmemAllocatorClient() = default;

fuchsia::sysmem2::BufferCollectionTokenPtr
SysmemAllocatorClient::CreateNewToken() {
  fuchsia::sysmem2::BufferCollectionTokenPtr collection_token;
  allocator_->AllocateSharedCollection(
      std::move(fuchsia::sysmem2::AllocatorAllocateSharedCollectionRequest{}
                    .set_token_request(collection_token.NewRequest())));
  return collection_token;
}

std::unique_ptr<SysmemCollectionClient>
SysmemAllocatorClient::AllocateNewCollection() {
  return std::make_unique<SysmemCollectionClient>(allocator_.get(),
                                                  CreateNewToken());
}

std::unique_ptr<SysmemCollectionClient>
SysmemAllocatorClient::BindSharedCollection(
    fuchsia::sysmem2::BufferCollectionTokenPtr token) {
  return std::make_unique<SysmemCollectionClient>(allocator_.get(),
                                                  std::move(token));
}

}  // namespace media
