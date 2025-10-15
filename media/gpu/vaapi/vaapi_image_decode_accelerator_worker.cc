// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decode_accelerator_worker.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_webp_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/webp_parser.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "string.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

VaapiImageDecoder* VaapiImageDecodeAcceleratorWorker::GetInitializedDecoder(
    const std::vector<uint8_t>& encoded_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  return nullptr;
}

// static
std::unique_ptr<VaapiImageDecodeAcceleratorWorker>
VaapiImageDecodeAcceleratorWorker::Create() {
  VaapiImageDecoderVector decoders;
  gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles;

  std::optional<gpu::ImageDecodeAcceleratorSupportedProfile>
      jpeg_decoder_supported_profile = VaapiJpegDecoder::GetSupportedProfile();
  if (jpeg_decoder_supported_profile) {
    decoders.push_back(std::make_unique<VaapiJpegDecoder>());
    supported_profiles.push_back(*jpeg_decoder_supported_profile);
  }

  std::optional<gpu::ImageDecodeAcceleratorSupportedProfile>
      webp_decoder_supported_profile = VaapiWebPDecoder::GetSupportedProfile();
  if (webp_decoder_supported_profile) {
    decoders.push_back(std::make_unique<VaapiWebPDecoder>());
    supported_profiles.push_back(*webp_decoder_supported_profile);
  }

  // If there are no decoders due to no supported profiles found, return
  // nullptr.
  if (decoders.empty()) {
    return nullptr;
  }

  return base::WrapUnique(new VaapiImageDecodeAcceleratorWorker(
      std::move(decoders), std::move(supported_profiles)));
}

VaapiImageDecodeAcceleratorWorker::VaapiImageDecodeAcceleratorWorker(
    VaapiImageDecoderVector decoders,
    gpu::ImageDecodeAcceleratorSupportedProfiles supported_profiles)
    : supported_profiles_(std::move(supported_profiles)) {
  DETACH_FROM_SEQUENCE(io_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  decoder_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
  CHECK(decoder_task_runner_);

  CHECK(!supported_profiles_.empty());
  CHECK(!decoders.empty());
  CHECK_EQ(decoders.size(), supported_profiles_.size());
  for (auto& decoder : decoders) {
    const gpu::ImageDecodeAcceleratorType type = decoder->GetType();
    decoders_[type] = std::move(decoder);
  }
}

void VaapiImageDecodeAcceleratorWorker::Destroy() {
  CHECK(decoder_task_runner_);
  decoder_task_runner_->DeleteSoon(FROM_HERE, this);
}

VaapiImageDecodeAcceleratorWorker::~VaapiImageDecodeAcceleratorWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  decoders_.clear();
}

gpu::ImageDecodeAcceleratorSupportedProfiles
VaapiImageDecodeAcceleratorWorker::GetSupportedProfiles() {
  return supported_profiles_;
}

void VaapiImageDecodeAcceleratorWorker::DecodeTask(
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size_for_tracing,
    gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  TRACE_EVENT2("jpeg", "VaapiImageDecodeAcceleratorWorker::DecodeTask",
               "encoded_bytes", encoded_data.size(), "output_size_for_tracing",
               output_size_for_tracing.ToString());
  gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB scoped_decode_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(decode_cb),
                                                  nullptr);
  VaapiImageDecoder* decoder = GetInitializedDecoder(encoded_data);
  // Decode into a VAAPI surface.
  if (!decoder) {
    DVLOGF(1) << "No decoder is available for supplied image";
    return;
  }
  VaapiImageDecodeStatus status = decoder->Decode(encoded_data);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    DVLOGF(1) << "Failed to decode - status = "
              << static_cast<uint32_t>(status);
    return;
  }

  // Export the decode result as a NativePixmap.
  std::unique_ptr<NativePixmapAndSizeInfo> exported_pixmap =
      decoder->ExportAsNativePixmapDmaBuf(&status);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    DVLOGF(1) << "Failed to export surface - status = "
              << static_cast<uint32_t>(status);
    return;
  }
  DCHECK(exported_pixmap);
  DCHECK(exported_pixmap->pixmap);
  if (exported_pixmap->pixmap->GetBufferSize() != output_size_for_tracing) {
    DVLOGF(1) << "Scaling is not supported";
    return;
  }

  // Output the decoded data.
  gfx::NativePixmapHandle pixmap_handle =
      exported_pixmap->pixmap->ExportHandle();
  // If a dup() failed while exporting the handle, we would get no planes.
  if (pixmap_handle.planes.empty()) {
    DVLOGF(1) << "Could not export the NativePixmapHandle";
    return;
  }
  auto result =
      std::make_unique<gpu::ImageDecodeAcceleratorWorker::DecodeResult>();
  result->handle = gfx::GpuMemoryBufferHandle(std::move(pixmap_handle));
  result->visible_size = exported_pixmap->pixmap->GetBufferSize();
  result->si_format = exported_pixmap->pixmap->GetSharedImageFormat();
  result->buffer_byte_size = exported_pixmap->byte_size;
  result->yuv_color_space = decoder->GetYUVColorSpace();
  std::move(scoped_decode_callback).Run(std::move(result));
}

void VaapiImageDecodeAcceleratorWorker::Decode(
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size,
    CompletedDecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  CHECK(decoder_task_runner_);

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiImageDecodeAcceleratorWorker::DecodeTask,
                                decoder_weak_this_factory_.GetWeakPtr(),
                                std::move(encoded_data), output_size,
                                std::move(decode_cb)));
}

}  // namespace media

namespace std {

void default_delete<media::VaapiImageDecodeAcceleratorWorker>::operator()(
    media::VaapiImageDecodeAcceleratorWorker* ptr) const {
  ptr->Destroy();
}

}  // namespace std
