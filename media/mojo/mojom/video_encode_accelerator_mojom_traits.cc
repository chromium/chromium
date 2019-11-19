// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"

#include "base/logging.h"
#include "base/optional.h"
#include "media/base/video_bitrate_allocation.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
media::mojom::VideoEncodeAccelerator::Error
EnumTraits<media::mojom::VideoEncodeAccelerator::Error,
           media::VideoEncodeAccelerator::Error>::
    ToMojom(media::VideoEncodeAccelerator::Error error) {
  switch (error) {
    case media::VideoEncodeAccelerator::kIllegalStateError:
      return media::mojom::VideoEncodeAccelerator::Error::ILLEGAL_STATE;
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      return media::mojom::VideoEncodeAccelerator::Error::INVALID_ARGUMENT;
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      return media::mojom::VideoEncodeAccelerator::Error::PLATFORM_FAILURE;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAccelerator::Error::INVALID_ARGUMENT;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAccelerator::Error,
                media::VideoEncodeAccelerator::Error>::
    FromMojom(media::mojom::VideoEncodeAccelerator::Error error,
              media::VideoEncodeAccelerator::Error* out) {
  switch (error) {
    case media::mojom::VideoEncodeAccelerator::Error::ILLEGAL_STATE:
      *out = media::VideoEncodeAccelerator::kIllegalStateError;
      return true;
    case media::mojom::VideoEncodeAccelerator::Error::INVALID_ARGUMENT:
      *out = media::VideoEncodeAccelerator::kInvalidArgumentError;
      return true;
    case media::mojom::VideoEncodeAccelerator::Error::PLATFORM_FAILURE:
      *out = media::VideoEncodeAccelerator::kPlatformFailureError;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
std::vector<int32_t> StructTraits<media::mojom::VideoBitrateAllocationDataView,
                                  media::VideoBitrateAllocation>::
    bitrates(const media::VideoBitrateAllocation& bitrate_allocation) {
  std::vector<int32_t> bitrates;
  int sum_bps = 0;
  for (size_t si = 0; si < media::VideoBitrateAllocation::kMaxSpatialLayers;
       ++si) {
    for (size_t ti = 0; ti < media::VideoBitrateAllocation::kMaxTemporalLayers;
         ++ti) {
      if (sum_bps == bitrate_allocation.GetSumBps()) {
        // The rest is all zeros, no need to iterate further.
        return bitrates;
      }
      const int layer_bitrate = bitrate_allocation.GetBitrateBps(si, ti);
      bitrates.emplace_back(layer_bitrate);
      sum_bps += layer_bitrate;
    }
  }
  return bitrates;
}

// static
bool StructTraits<media::mojom::VideoBitrateAllocationDataView,
                  media::VideoBitrateAllocation>::
    Read(media::mojom::VideoBitrateAllocationDataView data,
         media::VideoBitrateAllocation* out_bitrate_allocation) {
  ArrayDataView<int32_t> bitrates;
  data.GetBitratesDataView(&bitrates);
  size_t size = bitrates.size();
  if (size > media::VideoBitrateAllocation::kMaxSpatialLayers *
                 media::VideoBitrateAllocation::kMaxTemporalLayers) {
    return false;
  }
  for (size_t i = 0; i < size; ++i) {
    const int32_t bitrate = bitrates[i];
    const size_t si = i / media::VideoBitrateAllocation::kMaxTemporalLayers;
    const size_t ti = i % media::VideoBitrateAllocation::kMaxTemporalLayers;
    if (!out_bitrate_allocation->SetBitrate(si, ti, bitrate)) {
      return false;
    }
  }
  return true;
}

// static
bool StructTraits<media::mojom::BitstreamBufferMetadataDataView,
                  media::BitstreamBufferMetadata>::
    Read(media::mojom::BitstreamBufferMetadataDataView data,
         media::BitstreamBufferMetadata* out_metadata) {
  out_metadata->payload_size_bytes = data.payload_size_bytes();
  out_metadata->key_frame = data.key_frame();
  if (!data.ReadTimestamp(&out_metadata->timestamp)) {
    return false;
  }
  return data.ReadVp8(&out_metadata->vp8);
}

// static
bool StructTraits<media::mojom::Vp8MetadataDataView, media::Vp8Metadata>::Read(
    media::mojom::Vp8MetadataDataView data,
    media::Vp8Metadata* out_metadata) {
  out_metadata->non_reference = data.non_reference();
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->layer_sync = data.layer_sync();
  return true;
}

// static
media::mojom::VideoEncodeAcceleratorConfig::StorageType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig::StorageType,
           media::VideoEncodeAccelerator::Config::StorageType>::
    ToMojom(media::VideoEncodeAccelerator::Config::StorageType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::StorageType::kDmabuf:
      return media::mojom::VideoEncodeAcceleratorConfig::StorageType::kDmabuf;
    case media::VideoEncodeAccelerator::Config::StorageType::kShmem:
      return media::mojom::VideoEncodeAcceleratorConfig::StorageType::kShmem;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAcceleratorConfig::StorageType::kShmem;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig::StorageType,
                media::VideoEncodeAccelerator::Config::StorageType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig::StorageType input,
              media::VideoEncodeAccelerator::Config::StorageType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig::StorageType::kShmem:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kShmem;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig::StorageType::kDmabuf:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kDmabuf;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::VideoEncodeAcceleratorConfig::ContentType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig::ContentType,
           media::VideoEncodeAccelerator::Config::ContentType>::
    ToMojom(media::VideoEncodeAccelerator::Config::ContentType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::ContentType::kDisplay:
      return media::mojom::VideoEncodeAcceleratorConfig::ContentType::kDisplay;
    case media::VideoEncodeAccelerator::Config::ContentType::kCamera:
      return media::mojom::VideoEncodeAcceleratorConfig::ContentType::kCamera;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAcceleratorConfig::ContentType::kCamera;
}
// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig::ContentType,
                media::VideoEncodeAccelerator::Config::ContentType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig::ContentType input,
              media::VideoEncodeAccelerator::Config::ContentType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig::ContentType::kCamera:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kCamera;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig::ContentType::kDisplay:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::VideoEncodeAcceleratorConfigDataView,
                  media::VideoEncodeAccelerator::Config>::
    Read(media::mojom::VideoEncodeAcceleratorConfigDataView input,
         media::VideoEncodeAccelerator::Config* output) {
  media::VideoPixelFormat input_format;
  if (!input.ReadInputFormat(&input_format))
    return false;

  gfx::Size input_visible_size;
  if (!input.ReadInputVisibleSize(&input_visible_size))
    return false;

  media::VideoCodecProfile output_profile;
  if (!input.ReadOutputProfile(&output_profile))
    return false;

  base::Optional<uint32_t> initial_framerate;
  if (input.has_initial_framerate())
    initial_framerate = input.initial_framerate();

  base::Optional<uint32_t> gop_length;
  if (input.has_gop_length())
    gop_length = input.gop_length();

  base::Optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level())
    h264_output_level = input.h264_output_level();

  base::Optional<media::VideoEncodeAccelerator::Config::StorageType>
      storage_type;
  if (input.has_storage_type()) {
    if (!input.ReadStorageType(&storage_type))
      return false;
  }

  media::VideoEncodeAccelerator::Config::ContentType content_type;
  if (!input.ReadContentType(&content_type))
    return false;

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, input.initial_bitrate(),
      initial_framerate, gop_length, h264_output_level, storage_type,
      content_type);
  return true;
}

}  // namespace mojo
