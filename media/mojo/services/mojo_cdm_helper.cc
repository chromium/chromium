// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include "base/macros.h"
#include "base/stl_util.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

MojoCdmHelper::MojoCdmHelper(mojom::FrameInterfaceFactory* frame_interfaces)
    : frame_interfaces_(frame_interfaces) {}

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

url::Origin MojoCdmHelper::GetCdmOrigin() {
  url::Origin cdm_origin;
  // Since the CDM is created asynchronously, by the time this function is
  // called, the render frame host in the browser process may already be gone.
  // It's safe to ignore the error since the origin is used for crash reporting.
  ignore_result(frame_interfaces_->GetCdmOrigin(&cdm_origin));
  return cdm_origin;
}

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
    frame_interfaces_->CreateCdmStorage(
        cdm_storage_remote_.BindNewPipeAndPassReceiver());
  }
}

CdmAllocator* MojoCdmHelper::GetAllocator() {
  if (!allocator_)
    allocator_ = std::make_unique<MojoCdmAllocator>();
  return allocator_.get();
}

void MojoCdmHelper::ConnectToOutputProtection() {
  if (!output_protection_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        output_protection_.BindNewPipeAndPassReceiver()));
  }
}

void MojoCdmHelper::ConnectToPlatformVerification() {
  if (!platform_verification_) {
    frame_interfaces_->BindEmbedderReceiver(mojo::GenericPendingReceiver(
        platform_verification_.BindNewPipeAndPassReceiver()));
  }
}

}  // namespace media
