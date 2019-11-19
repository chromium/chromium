// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/image_decode_accelerator_proxy.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "cc/paint/paint_image.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

namespace {

// TODO(crbug.com/984971): for WebPs we may need to compute the coded size
// instead and check that against the supported dimensions.
bool IsSupportedImageSize(
    const cc::ImageHeaderMetadata* image_data,
    const ImageDecodeAcceleratorSupportedProfile& supported_profile) {
  DCHECK(image_data);

  gfx::Size image_size;
  if (image_data->coded_size.has_value())
    image_size = image_data->coded_size.value();
  else
    image_size = image_data->image_size;
  DCHECK(!image_size.IsEmpty());

  return image_size.width() >=
             supported_profile.min_encoded_dimensions.width() &&
         image_size.height() >=
             supported_profile.min_encoded_dimensions.height() &&
         image_size.width() <=
             supported_profile.max_encoded_dimensions.width() &&
         image_size.height() <=
             supported_profile.max_encoded_dimensions.height();
}

bool IsSupportedJpegImage(
    const cc::ImageHeaderMetadata* image_data,
    const ImageDecodeAcceleratorSupportedProfile& supported_profile) {
  DCHECK(image_data);
  DCHECK_EQ(cc::ImageType::kJPEG, image_data->image_type);
  DCHECK_EQ(ImageDecodeAcceleratorType::kJpeg, supported_profile.image_type);

  DCHECK(image_data->jpeg_is_progressive.has_value());
  if (image_data->jpeg_is_progressive.value())
    return false;

  // Check the chroma subsampling format.
  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of ImageDecodeAcceleratorSubsampling to make sure it
      // has the same number of entries as cc::YUVSubsampling.
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "IsSupportedJpegImage() must be adapted to support all subsampling "
      "factors in ImageDecodeAcceleratorSubsampling");
  ImageDecodeAcceleratorSubsampling subsampling;
  switch (image_data->yuv_subsampling) {
    case cc::YUVSubsampling::k420:
      subsampling = ImageDecodeAcceleratorSubsampling::k420;
      break;
    case cc::YUVSubsampling::k422:
      subsampling = ImageDecodeAcceleratorSubsampling::k422;
      break;
    case cc::YUVSubsampling::k444:
      subsampling = ImageDecodeAcceleratorSubsampling::k444;
      break;
    default:
      // Currently not supporting any other subsampling format.
      return false;
  }

  return std::find(supported_profile.subsamplings.cbegin(),
                   supported_profile.subsamplings.cend(),
                   subsampling) != supported_profile.subsamplings.cend();
}

}  // namespace

ImageDecodeAcceleratorProxy::ImageDecodeAcceleratorProxy(GpuChannelHost* host,
                                                         int32_t route_id)
    : host_(host), route_id_(route_id) {}

ImageDecodeAcceleratorProxy::~ImageDecodeAcceleratorProxy() {}

bool ImageDecodeAcceleratorProxy::IsImageSupported(
    const cc::ImageHeaderMetadata* image_metadata) const {
  DCHECK(host_);

  if (!image_metadata)
    return false;

  // Image is supported only if all the encoded data was received prior to any
  // decoding work. Otherwise, it means that the software decoder has already
  // started decoding the image, so we just let it finish.
  if (!image_metadata->all_data_received_prior_to_decode)
    return false;

  // Image is supported only if the image doesn't have an embedded color space.
  // This is because we don't currently send the embedded color profile with the
  // decode request.
  if (image_metadata->has_embedded_color_profile)
    return false;

  static_assert(static_cast<int>(ImageDecodeAcceleratorType::kMaxValue) == 2,
                "IsImageSupported() must be adapted to support all image types "
                "in ImageDecodeAcceleratorType");
  ImageDecodeAcceleratorType image_type = ImageDecodeAcceleratorType::kUnknown;
  switch (image_metadata->image_type) {
    case cc::ImageType::kJPEG:
      image_type = ImageDecodeAcceleratorType::kJpeg;
      break;
    case cc::ImageType::kWEBP:
      image_type = ImageDecodeAcceleratorType::kWebP;
      break;
    default:
      return false;
  }

  // Find the image decode accelerator supported profile according to the type
  // of the image.
  const std::vector<ImageDecodeAcceleratorSupportedProfile>& profiles =
      host_->gpu_info().image_decode_accelerator_supported_profiles;
  auto profile_it = std::find_if(
      profiles.cbegin(), profiles.cend(),
      [image_type](const ImageDecodeAcceleratorSupportedProfile& profile) {
        return profile.image_type == image_type;
      });
  if (profile_it == profiles.cend())
    return false;

  // Verify that the image size is supported.
  if (!IsSupportedImageSize(image_metadata, *profile_it))
    return false;

  // Validate the image according to that profile.
  switch (image_type) {
    case ImageDecodeAcceleratorType::kJpeg:
      return IsSupportedJpegImage(image_metadata, *profile_it);
    case ImageDecodeAcceleratorType::kWebP:
      DCHECK(image_metadata->webp_is_non_extended_lossy.has_value());
      return image_metadata->webp_is_non_extended_lossy.value();
    case ImageDecodeAcceleratorType::kUnknown:
      // Should not reach due to a check above.
      NOTREACHED();
      break;
  }
  return false;
}

bool ImageDecodeAcceleratorProxy::IsJpegDecodeAccelerationSupported() const {
  const auto& profiles =
      host_->gpu_info().image_decode_accelerator_supported_profiles;
  for (const auto& profile : profiles) {
    if (profile.image_type == ImageDecodeAcceleratorType::kJpeg)
      return true;
  }
  return false;
}

bool ImageDecodeAcceleratorProxy::IsWebPDecodeAccelerationSupported() const {
  const auto& profiles =
      host_->gpu_info().image_decode_accelerator_supported_profiles;
  for (const auto& profile : profiles) {
    if (profile.image_type == ImageDecodeAcceleratorType::kWebP)
      return true;
  }
  return false;
}

SyncToken ImageDecodeAcceleratorProxy::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    CommandBufferId raster_decoder_command_buffer_id,
    uint32_t transfer_cache_entry_id,
    int32_t discardable_handle_shm_id,
    uint32_t discardable_handle_shm_offset,
    uint64_t discardable_handle_release_count,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  DCHECK(host_);
  DCHECK_EQ(host_->channel_id(),
            ChannelIdFromCommandBufferId(raster_decoder_command_buffer_id));

  GpuChannelMsg_ScheduleImageDecode_Params params;
  params.encoded_data =
      std::vector<uint8_t>(encoded_data.cbegin(), encoded_data.cend());
  params.output_size = output_size;
  params.raster_decoder_route_id =
      RouteIdFromCommandBufferId(raster_decoder_command_buffer_id);
  params.transfer_cache_entry_id = transfer_cache_entry_id;
  params.discardable_handle_shm_id = discardable_handle_shm_id;
  params.discardable_handle_shm_offset = discardable_handle_shm_offset;
  params.discardable_handle_release_count = discardable_handle_release_count;
  params.target_color_space = target_color_space;
  params.needs_mips = needs_mips;

  base::AutoLock lock(lock_);
  const uint64_t release_count = ++next_release_count_;
  // Note: we send the message under the lock to guarantee monotonicity of the
  // release counts as seen by the service.
  // The EnsureFlush() call makes sure that the sync token corresponding to
  // |discardable_handle_release_count| is visible to the service before
  // processing the image decode request.
  host_->EnsureFlush(UINT32_MAX);
  host_->Send(new GpuChannelMsg_ScheduleImageDecode(
      route_id_, std::move(params), release_count));
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      release_count);
}

}  // namespace gpu
