// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "base/stl_util.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/connect.h"

namespace media {

MojoCdmHelper::MojoCdmHelper(
    service_manager::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {}

MojoCdmHelper::~MojoCdmHelper() = default;

void MojoCdmHelper::SetFileReadCB(FileReadCB file_read_cb) {
  file_read_cb_ = std::move(file_read_cb);
}

cdm::FileIO* MojoCdmHelper::CreateCdmFileIO(cdm::FileIOClient* client) {
  ConnectToCdmStorage();

  // Pass a reference to CdmStorage so that MojoCdmFileIO can open a file.
  auto mojo_cdm_file_io =
      std::make_unique<MojoCdmFileIO>(this, client, cdm_storage_remote_.get());

  cdm::FileIO* cdm_file_io = mojo_cdm_file_io.get();
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;

  cdm_file_io_set_.push_back(std::move(mojo_cdm_file_io));
  return cdm_file_io;
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
cdm::CdmProxy* MojoCdmHelper::CreateCdmProxy(cdm::CdmProxyClient* client) {
  DVLOG(3) << __func__;
  if (cdm_proxy_) {
    DVLOG(1) << __func__ << ": Only one outstanding CdmProxy allowed.";
    return nullptr;
  }

  mojo::PendingRemote<mojom::CdmProxy> cdm_proxy_remote;
  service_manager::GetInterface<mojom::CdmProxy>(
      interface_provider_, cdm_proxy_remote.InitWithNewPipeAndPassReceiver());
  cdm_proxy_ =
      std::make_unique<MojoCdmProxy>(std::move(cdm_proxy_remote), client);
  return cdm_proxy_.get();
}

int MojoCdmHelper::GetCdmProxyCdmId() {
  return cdm_proxy_ ? cdm_proxy_->GetCdmId() : CdmContext::kInvalidCdmId;
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

cdm::Buffer* MojoCdmHelper::CreateCdmBuffer(size_t capacity) {
  return GetAllocator()->CreateCdmBuffer(capacity);
}

std::unique_ptr<VideoFrameImpl> MojoCdmHelper::CreateCdmVideoFrame() {
  return GetAllocator()->CreateCdmVideoFrame();
}

void MojoCdmHelper::QueryStatus(QueryStatusCB callback) {
  QueryStatusCB scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), false, 0, 0);
  ConnectToOutputProtection();
  output_protection_->QueryStatus(std::move(scoped_callback));
}

void MojoCdmHelper::EnableProtection(uint32_t desired_protection_mask,
                                     EnableProtectionCB callback) {
  EnableProtectionCB scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  ConnectToOutputProtection();
  output_protection_->EnableProtection(desired_protection_mask,
                                       std::move(scoped_callback));
}

void MojoCdmHelper::ChallengePlatform(const std::string& service_id,
                                      const std::string& challenge,
                                      ChallengePlatformCB callback) {
  ChallengePlatformCB scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false,
                                                  "", "", "");
  ConnectToPlatformVerification();
  platform_verification_->ChallengePlatform(service_id, challenge,
                                            std::move(scoped_callback));
}

void MojoCdmHelper::GetStorageId(uint32_t version, StorageIdCB callback) {
  StorageIdCB scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), version, std::vector<uint8_t>());
  ConnectToPlatformVerification();
  platform_verification_->GetStorageId(version, std::move(scoped_callback));
}

void MojoCdmHelper::CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) {
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;
  base::EraseIf(cdm_file_io_set_,
                [cdm_file_io](const std::unique_ptr<MojoCdmFileIO>& ptr) {
                  return ptr.get() == cdm_file_io;
                });
}

void MojoCdmHelper::ReportFileReadSize(int file_size_bytes) {
  DVLOG(3) << __func__ << ": file_size_bytes = " << file_size_bytes;
  if (file_read_cb_)
    file_read_cb_.Run(file_size_bytes);
}

void MojoCdmHelper::ConnectToCdmStorage() {
  if (!cdm_storage_remote_) {
    service_manager::GetInterface<mojom::CdmStorage>(
        interface_provider_, cdm_storage_remote_.BindNewPipeAndPassReceiver());
  }
}

CdmAllocator* MojoCdmHelper::GetAllocator() {
  if (!allocator_)
    allocator_ = std::make_unique<MojoCdmAllocator>();
  return allocator_.get();
}

void MojoCdmHelper::ConnectToOutputProtection() {
  if (!output_protection_) {
    service_manager::GetInterface<mojom::OutputProtection>(
        interface_provider_, output_protection_.BindNewPipeAndPassReceiver());
  }
}

void MojoCdmHelper::ConnectToPlatformVerification() {
  if (!platform_verification_) {
    interface_provider_->GetInterface(
        mojom::PlatformVerification::Name_,
        platform_verification_.BindNewPipeAndPassReceiver().PassPipe());
  }
}

}  // namespace media
