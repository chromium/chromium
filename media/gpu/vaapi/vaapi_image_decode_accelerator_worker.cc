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

}  // namespace media

namespace std {

void default_delete<media::VaapiImageDecodeAcceleratorWorker>::operator()(
    media::VaapiImageDecodeAcceleratorWorker* ptr) const {
  ptr->Destroy();
}

}  // namespace std
