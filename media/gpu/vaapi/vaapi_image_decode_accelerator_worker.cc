// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decode_accelerator_worker.h"

#include "string.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_webp_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/webp_parser.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {

namespace {

bool IsJpegImage(base::span<const uint8_t> encoded_data) {
  if (encoded_data.size() < 3u)
    return false;
  return memcmp("\xFF\xD8\xFF", encoded_data.data(), 3u) == 0;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VAJDAWorkerDecoderFailure {
  kVaapiError = 0,
  kMaxValue = kVaapiError,
};

void ReportToVAJDAWorkerDecoderFailureUMA(VAJDAWorkerDecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDAWorker.DecoderFailure", failure);
}

// Uses |decoder| to decode the image corresponding to |encoded_data|.
// |decode_cb| is called when finished or when an error is encountered. We don't
// support decoding to scale, so |output_size| is only used for tracing.
void DecodeTask(
    VaapiImageDecoder* decoder,
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size,
    gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB decode_cb) {
  TRACE_EVENT2("jpeg", "VaapiImageDecodeAcceleratorWorker::DecodeTask",
               "encoded_bytes", encoded_data.size(), "output_size",
               output_size.ToString());
  gpu::ImageDecodeAcceleratorWorker::CompletedDecodeCB scoped_decode_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(decode_cb),
                                                  nullptr);

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
  if (exported_pixmap->pixmap->GetBufferSize() != output_size) {
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
  result->handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
  result->handle.native_pixmap_handle = std::move(pixmap_handle);
  result->visible_size = exported_pixmap->pixmap->GetBufferSize();
  result->buffer_format = exported_pixmap->pixmap->GetBufferFormat();
  result->buffer_byte_size = exported_pixmap->byte_size;
  result->yuv_color_space = decoder->GetYUVColorSpace();
  std::move(scoped_decode_callback).Run(std::move(result));
}

}  // namespace

// static
std::unique_ptr<VaapiImageDecodeAcceleratorWorker>
VaapiImageDecodeAcceleratorWorker::Create() {
  // TODO(crbug.com/988123): revisit the Media.VAJDAWorker.DecoderFailure UMA
  // to be able to record WebP and JPEG failures separately.
  const auto uma_cb =
      base::BindRepeating(&ReportToVAJDAWorkerDecoderFailureUMA,
                          VAJDAWorkerDecoderFailure::kVaapiError);
  VaapiImageDecoderVector decoders;

  auto jpeg_decoder = std::make_unique<VaapiJpegDecoder>();
  // TODO(crbug.com/974438): we can't advertise accelerated image decoding in
  // AMD until we support VAAPI surfaces with multiple buffer objects.
  if (VaapiWrapper::GetImplementationType() != VAImplementation::kMesaGallium &&
      jpeg_decoder->Initialize(uma_cb)) {
    decoders.push_back(std::move(jpeg_decoder));
  }

  auto webp_decoder = std::make_unique<VaapiWebPDecoder>();
  if (webp_decoder->Initialize(uma_cb))
    decoders.push_back(std::move(webp_decoder));

  // If there are no decoders due to disabled flags or initialization failure,
  // return nullptr.
  if (decoders.empty())
    return nullptr;

  return base::WrapUnique(
      new VaapiImageDecodeAcceleratorWorker(std::move(decoders)));
}

VaapiImageDecodeAcceleratorWorker::VaapiImageDecodeAcceleratorWorker(
    VaapiImageDecoderVector decoders) {
  DETACH_FROM_SEQUENCE(io_sequence_checker_);
  decoder_task_runner_ = base::CreateSequencedTaskRunner({base::ThreadPool()});
  DCHECK(decoder_task_runner_);

  DCHECK(!decoders.empty());
  for (auto& decoder : decoders) {
    supported_profiles_.push_back(decoder->GetSupportedProfile());
    const gpu::ImageDecodeAcceleratorType type = decoder->GetType();
    decoders_[type] = std::move(decoder);
  }
}

VaapiImageDecodeAcceleratorWorker::~VaapiImageDecodeAcceleratorWorker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(decoder_task_runner_);
  for (auto& decoder : decoders_)
    decoder_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder.second));
}

gpu::ImageDecodeAcceleratorSupportedProfiles
VaapiImageDecodeAcceleratorWorker::GetSupportedProfiles() {
  return supported_profiles_;
}

VaapiImageDecoder* VaapiImageDecodeAcceleratorWorker::GetDecoderForImage(
    const std::vector<uint8_t>& encoded_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  auto result = decoders_.end();

  if (base::FeatureList::IsEnabled(
          features::kVaapiJpegImageDecodeAcceleration) &&
      IsJpegImage(encoded_data)) {
    result = decoders_.find(gpu::ImageDecodeAcceleratorType::kJpeg);
  } else if (base::FeatureList::IsEnabled(
                 features::kVaapiWebPImageDecodeAcceleration) &&
             IsLossyWebPImage(encoded_data)) {
    result = decoders_.find(gpu::ImageDecodeAcceleratorType::kWebP);
  }

  return result == decoders_.end() ? nullptr : result->second.get();
}

void VaapiImageDecodeAcceleratorWorker::Decode(
    std::vector<uint8_t> encoded_data,
    const gfx::Size& output_size,
    CompletedDecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  DCHECK(decoder_task_runner_);

  // We defer checking for a null |decoder| until DecodeTask() because the
  // gpu::ImageDecodeAcceleratorWorker interface mandates that the callback be
  // called asynchronously.
  VaapiImageDecoder* decoder = GetDecoderForImage(encoded_data);
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DecodeTask, decoder, std::move(encoded_data),
                                output_size, std::move(decode_cb)));
}

}  // namespace media
