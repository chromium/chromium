// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_helper.h"

#include <tuple>

#include "build/build_config.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_helpers.h"
#include "media/mojo/services/mojo_cdm_allocator.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace media {

MojoCdmHelper::MojoCdmHelper(mojom::FrameInterfaceFactory* frame_interfaces)
    : frame_interfaces_(frame_interfaces) {
  // Retrieve the Ukm recording objects in the constructor of MojoCdmHelper
  // because the connection to the renderer frame host might be disconnected at
  // any time and we record the Ukm in the destructor of CdmAdapter, so we
  // should store the objects as early as possible.
  RetrieveUkmRecordingObjects();
}

MojoCdmHelper::~MojoCdmHelper() = default;

void MojoCdmHelper::SetFileReadCB(FileReadCB file_read_cb) {
  file_read_cb_ = std::move(file_read_cb);
}

cdm::FileIO* MojoCdmHelper::CreateCdmFileIO(cdm::FileIOClient* client) {
  mojo::Remote<mojom::CdmStorage> cdm_storage;
  frame_interfaces_->CreateCdmStorage(cdm_storage.BindNewPipeAndPassReceiver());
  // No reset_on_disconnect() since when document is destroyed the CDM should be
  // destroyed as well.

  auto mojo_cdm_file_io =
      std::make_unique<MojoCdmFileIO>(this, client, std::move(cdm_storage));

  cdm::FileIO* cdm_file_io = mojo_cdm_file_io.get();
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;

  cdm_file_io_set_.push_back(std::move(mojo_cdm_file_io));
  return cdm_file_io;
}

url::Origin MojoCdmHelper::GetCdmOrigin() {
  url::Origin cdm_origin;
  // Since the CDM is created asynchronously, by the time this function is
  // called, the RenderFrameHost in the browser process may already be gone.
  // It's safe to ignore the error since the origin is used for crash reporting.
  std::ignore = frame_interfaces_->GetCdmOrigin(&cdm_origin);
  return cdm_origin;
}

#if BUILDFLAG(IS_WIN)
void MojoCdmHelper::GetMediaFoundationCdmData(
    GetMediaFoundationCdmDataCB callback) {
  ConnectToCdmDocumentService();
  cdm_document_service_->GetMediaFoundationCdmData(std::move(callback));
}

void MojoCdmHelper::SetCdmClientToken(
    const std::vector<uint8_t>& client_token) {
  ConnectToCdmDocumentService();
  cdm_document_service_->SetCdmClientToken(client_token);
}

void MojoCdmHelper::OnCdmEvent(CdmEvent event, HRESULT hresult) {
  ConnectToCdmDocumentService();
  cdm_document_service_->OnCdmEvent(event, hresult);
}
#endif  // BUILDFLAG(IS_WIN)

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
  ConnectToCdmDocumentService();
  cdm_document_service_->ChallengePlatform(service_id, challenge,
                                           std::move(scoped_callback));
}

void MojoCdmHelper::GetStorageId(uint32_t version, StorageIdCB callback) {
  StorageIdCB scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), version, std::vector<uint8_t>());
  ConnectToCdmDocumentService();
  cdm_document_service_->GetStorageId(version, std::move(scoped_callback));
}

void MojoCdmHelper::CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) {
  DVLOG(3) << __func__ << ": cdm_file_io = " << cdm_file_io;
  std::erase_if(cdm_file_io_set_,
                [cdm_file_io](const std::unique_ptr<MojoCdmFileIO>& ptr) {
                  return ptr.get() == cdm_file_io;
                });
}

void MojoCdmHelper::ReportFileReadSize(int file_size_bytes) {
  DVLOG(3) << __func__ << ": file_size_bytes = " << file_size_bytes;
  if (file_read_cb_)
    file_read_cb_.Run(file_size_bytes);
}

void MojoCdmHelper::RecordUkm(const CdmMetricsData& cdm_metrics_data) {
  ukm::SourceId source_id = ukm::ConvertToSourceId(ukm::AssignNewSourceId(),
                                                   ukm::SourceIdType::CDM_ID);
  ukm_recorder_->UpdateSourceURL(source_id,
                                 cdm_metrics_data.cdm_origin.GetURL());

  auto ukm_builder = ukm::builders::Media_EME_CdmMetrics(source_id);

  if (cdm_metrics_data.license_sdk_version.has_value()) {
    ukm_builder.SetLicenseSdkVersion(
        cdm_metrics_data.license_sdk_version.value());
  }

  ukm_builder.SetNumberOfUpdateCalls(cdm_metrics_data.number_of_update_calls);

  ukm_builder.SetNumberOfOnMessageEvents(
      cdm_metrics_data.number_of_on_message_events);

  if (cdm_metrics_data.certificate_serial_number.has_value()) {
    ukm_builder.SetCertificateSerialNumber(
        cdm_metrics_data.certificate_serial_number.value());
  }

  if (cdm_metrics_data.decoder_bypass_block_count.has_value()) {
    ukm_builder.SetDecoderBypassBlockCount(
        cdm_metrics_data.decoder_bypass_block_count.value());
  }

  ukm_builder.SetNumberOfVideoFrames(cdm_metrics_data.video_frames_processed);

  ukm_builder.Record(ukm_recorder_.get());
}

void MojoCdmHelper::RetrieveUkmRecordingObjects() {
  ConnectToUkmRecorderFactory();

  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*ukm_recorder_factory_);
}

void MojoCdmHelper::ConnectToOutputProtection() {
  if (!output_protection_) {
    DVLOG(2) << "Connect to mojom::OutputProtection";
    frame_interfaces_->BindEmbedderReceiver(
        output_protection_.BindNewPipeAndPassReceiver());
    // No reset_on_disconnect() since MediaInterfaceProxy should be destroyed
    // when document is destroyed, which will destroy MojoCdmHelper as well.
  }
}

void MojoCdmHelper::ConnectToCdmDocumentService() {
  if (!cdm_document_service_) {
    DVLOG(2) << "Connect to mojom::CdmDocumentService";
    frame_interfaces_->BindEmbedderReceiver(
        cdm_document_service_.BindNewPipeAndPassReceiver());
    // No reset_on_disconnect() since MediaInterfaceProxy should be destroyed
    // when document is destroyed, which will destroy MojoCdmHelper as well.
  }
}

void MojoCdmHelper::ConnectToUkmRecorderFactory() {
  if (!ukm_recorder_factory_) {
    DVLOG(2) << "Connect to ukm::mojom::UkmRecorderFactory";
    frame_interfaces_->BindEmbedderReceiver(
        ukm_recorder_factory_.BindNewPipeAndPassReceiver());
    // No reset_on_disconnect() since MediaInterfaceProxy should be destroyed
    // when document is destroyed, which will destroy MojoCdmHelper as well.
  }
}

CdmAllocator* MojoCdmHelper::GetAllocator() {
  if (!allocator_)
    allocator_ = std::make_unique<MojoCdmAllocator>();
  return allocator_.get();
}

}  // namespace media
