// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"

#include <optional>

#include "base/notreached.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

// static
media::mojom::VideoEncodeAcceleratorSupportedRateControlMode
EnumTraits<media::mojom::VideoEncodeAcceleratorSupportedRateControlMode,
           media::VideoEncodeAccelerator::SupportedRateControlMode>::
    ToMojom(media::VideoEncodeAccelerator::SupportedRateControlMode mode) {
  switch (mode) {
    case media::VideoEncodeAccelerator::kNoMode:
      return media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
          kNoMode;
    case media::VideoEncodeAccelerator::kConstantMode:
      return media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
          kConstantMode;
    case media::VideoEncodeAccelerator::kVariableMode:
      return media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
          kVariableMode;
    case media::VideoEncodeAccelerator::kExternalMode:
      return media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
          kExternalMode;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorSupportedRateControlMode,
                media::VideoEncodeAccelerator::SupportedRateControlMode>::
    FromMojom(media::mojom::VideoEncodeAcceleratorSupportedRateControlMode mode,
              media::VideoEncodeAccelerator::SupportedRateControlMode* out) {
  switch (mode) {
    case media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::kNoMode:
      *out = media::VideoEncodeAccelerator::kNoMode;
      return true;
    case media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
        kConstantMode:
      *out = media::VideoEncodeAccelerator::kConstantMode;
      return true;
    case media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
        kVariableMode:
      *out = media::VideoEncodeAccelerator::kVariableMode;
      return true;
    case media::mojom::VideoEncodeAcceleratorSupportedRateControlMode::
        kExternalMode:
      *out = media::VideoEncodeAccelerator::kExternalMode;
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
                  media::VideoEncodeAccelerator::SupportedProfile>::
    Read(media::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
         media::VideoEncodeAccelerator::SupportedProfile* out) {
  if (!data.ReadMinResolution(&out->min_resolution) ||
      !data.ReadMaxResolution(&out->max_resolution) ||
      !data.ReadProfile(&out->profile)) {
    return false;
  }

  out->max_framerate_numerator = data.max_framerate_numerator();
  out->max_framerate_denominator = data.max_framerate_denominator();
  out->rate_control_modes = media::VideoEncodeAccelerator::kNoMode;
  std::vector<media::VideoEncodeAccelerator::SupportedRateControlMode> modes;
  if (!data.ReadRateControlModes(&modes))
    return false;
  for (const auto& mode : modes) {
    out->rate_control_modes |= mode;
  }

  std::vector<media::SVCScalabilityMode> scalability_modes;
  if (!data.ReadScalabilityModes(&scalability_modes))
    return false;
  out->scalability_modes = std::move(scalability_modes);

  out->is_software_codec = data.is_software_codec();

  std::vector<media::VideoPixelFormat> gpu_supported_pixel_formats;
  if (!data.ReadGpuSupportedPixelFormats(&gpu_supported_pixel_formats)) {
    return false;
  }
  out->gpu_supported_pixel_formats = std::move(gpu_supported_pixel_formats);
  out->supports_gpu_shared_images = data.supports_gpu_shared_images();
  return true;
}

// static
bool StructTraits<media::mojom::VariableBitratePeakDataView, uint32_t>::Read(
    media::mojom::VariableBitratePeakDataView data,
    uint32_t* out_peak_bps) {
  uint32_t peak_bps = data.bps();
  if (peak_bps == 0)
    return false;
  *out_peak_bps = peak_bps;
  return true;
}

// static
std::vector<uint32_t> StructTraits<media::mojom::VideoBitrateAllocationDataView,
                                   media::VideoBitrateAllocation>::
    bitrates(const media::VideoBitrateAllocation& bitrate_allocation) {
  std::vector<uint32_t> bitrates;
  uint32_t sum_bps = 0;
  for (size_t si = 0; si < media::VideoBitrateAllocation::kMaxSpatialLayers;
       ++si) {
    for (size_t ti = 0; ti < media::VideoBitrateAllocation::kMaxTemporalLayers;
         ++ti) {
      if (sum_bps == bitrate_allocation.GetSumBps()) {
        // The rest is all zeros, no need to iterate further.
        return bitrates;
      }
      const uint32_t layer_bitrate = bitrate_allocation.GetBitrateBps(si, ti);
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
  std::optional<uint32_t> peak_bps;
  if (!data.ReadVariableBitratePeak(&peak_bps))
    return false;
  if (peak_bps.has_value()) {
    *out_bitrate_allocation =
        media::VideoBitrateAllocation(media::Bitrate::Mode::kVariable);
  } else {
    *out_bitrate_allocation =
        media::VideoBitrateAllocation(media::Bitrate::Mode::kConstant);
  }
  ArrayDataView<uint32_t> bitrates;
  data.GetBitratesDataView(&bitrates);
  size_t size = bitrates.size();
  if (size > media::VideoBitrateAllocation::kMaxSpatialLayers *
                 media::VideoBitrateAllocation::kMaxTemporalLayers) {
    return false;
  }
  for (size_t i = 0; i < size; ++i) {
    const uint32_t bitrate = bitrates[i];
    const size_t si = i / media::VideoBitrateAllocation::kMaxTemporalLayers;
    const size_t ti = i % media::VideoBitrateAllocation::kMaxTemporalLayers;
    if (!out_bitrate_allocation->SetBitrate(si, ti, bitrate)) {
      return false;
    }
  }

  if (peak_bps.has_value()) {
    if (!out_bitrate_allocation->SetPeakBps(*peak_bps)) {
      // Invalid (too low) peak for the sum of the bitrates.
      return false;
    }
  }

  return true;
}

// static
bool StructTraits<media::mojom::VideoEncodeOptionsDataView,
                  media::VideoEncoder::EncodeOptions>::
    Read(media::mojom::VideoEncodeOptionsDataView data,
         media::VideoEncoder::EncodeOptions* out_options) {
  out_options->key_frame = data.force_keyframe();
  int32_t quantizer = data.quantizer();
  if (quantizer < 0) {
    out_options->quantizer.reset();
  } else {
    out_options->quantizer = data.quantizer();
  }

  return true;
}

// static
bool UnionTraits<media::mojom::OptionalMetadataDataView,
                 media::BitstreamBufferMetadata>::
    Read(media::mojom::OptionalMetadataDataView data,
         media::BitstreamBufferMetadata* out) {
  switch (data.tag()) {
    case media::mojom::OptionalMetadataDataView::Tag::kDrop: {
      return data.ReadDrop(&out->drop);
    }
    case media::mojom::OptionalMetadataDataView::Tag::kH264: {
      return data.ReadH264(&out->h264);
    }
    case media::mojom::OptionalMetadataDataView::Tag::kVp8: {
      return data.ReadVp8(&out->vp8);
    }
    case media::mojom::OptionalMetadataDataView::Tag::kVp9: {
      return data.ReadVp9(&out->vp9);
    }
    case media::mojom::OptionalMetadataDataView::Tag::kAv1: {
      return data.ReadAv1(&out->av1);
    }
    case media::mojom::OptionalMetadataDataView::Tag::kH265: {
      return data.ReadH265(&out->h265);
    }
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::BitstreamBufferMetadataDataView,
                  media::BitstreamBufferMetadata>::
    Read(media::mojom::BitstreamBufferMetadataDataView data,
         media::BitstreamBufferMetadata* metadata) {
  metadata->payload_size_bytes = data.payload_size_bytes();
  metadata->key_frame = data.key_frame();
  if (!data.ReadTimestamp(&metadata->timestamp)) {
    return false;
  }
  metadata->qp = data.qp();
  if (!data.ReadEncodedSize(&metadata->encoded_size)) {
    return false;
  }
  if (!data.ReadEncodedColorSpace(&metadata->encoded_color_space)) {
    return false;
  }

  return data.ReadOptionalMetadata(metadata);
}

// static
bool StructTraits<media::mojom::DropFrameMetadataDataView,
                  media::DropFrameMetadata>::
    Read(media::mojom::DropFrameMetadataDataView data,
         media::DropFrameMetadata* out_metadata) {
  out_metadata->spatial_idx = data.spatial_idx();
  out_metadata->end_of_picture = data.end_of_picture();
  return true;
}

// static
bool StructTraits<media::mojom::H264MetadataDataView, media::H264Metadata>::
    Read(media::mojom::H264MetadataDataView data,
         media::H264Metadata* out_metadata) {
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->layer_sync = data.layer_sync();
  return true;
}

// static
bool StructTraits<media::mojom::H265MetadataDataView, media::H265Metadata>::
    Read(media::mojom::H265MetadataDataView data,
         media::H265Metadata* out_metadata) {
  out_metadata->temporal_idx = data.temporal_idx();
  return true;
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
bool StructTraits<media::mojom::Vp9MetadataDataView, media::Vp9Metadata>::Read(
    media::mojom::Vp9MetadataDataView data,
    media::Vp9Metadata* out_metadata) {
  out_metadata->inter_pic_predicted = data.inter_pic_predicted();
  out_metadata->temporal_up_switch = data.temporal_up_switch();
  out_metadata->referenced_by_upper_spatial_layers =
      data.referenced_by_upper_spatial_layers();
  out_metadata->reference_lower_spatial_layers =
      data.reference_lower_spatial_layers();
  out_metadata->end_of_picture = data.end_of_picture();
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->spatial_idx = data.spatial_idx();
  out_metadata->begin_active_spatial_layer_index =
      data.begin_active_spatial_layer_index();
  out_metadata->end_active_spatial_layer_index =
      data.end_active_spatial_layer_index();
  return data.ReadSpatialLayerResolutions(
             &out_metadata->spatial_layer_resolutions) &&
         data.ReadPDiffs(&out_metadata->p_diffs);
}

// static
bool StructTraits<media::mojom::Av1MetadataDataView, media::Av1Metadata>::Read(
    media::mojom::Av1MetadataDataView data,
    media::Av1Metadata* out_metadata) {
  out_metadata->temporal_idx = data.temporal_idx();
  return true;
}

// static
media::mojom::VideoEncodeAcceleratorConfig_StorageType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_StorageType,
           media::VideoEncodeAccelerator::Config::StorageType>::
    ToMojom(media::VideoEncodeAccelerator::Config::StorageType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer:
      return media::mojom::VideoEncodeAcceleratorConfig_StorageType::
          kGpuMemoryBuffer;
    case media::VideoEncodeAccelerator::Config::StorageType::kShmem:
      return media::mojom::VideoEncodeAcceleratorConfig_StorageType::kShmem;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_StorageType,
                media::VideoEncodeAccelerator::Config::StorageType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig_StorageType input,
              media::VideoEncodeAccelerator::Config::StorageType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_StorageType::kShmem:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kShmem;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_StorageType::
        kGpuMemoryBuffer:
      *output =
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
      return true;
  }
  NOTREACHED();
}

// static
media::mojom::VideoEncodeAcceleratorConfig_EncoderType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_EncoderType,
           media::VideoEncodeAccelerator::Config::EncoderType>::
    ToMojom(media::VideoEncodeAccelerator::Config::EncoderType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::EncoderType::kHardware:
      return media::mojom::VideoEncodeAcceleratorConfig_EncoderType::kHardware;
    case media::VideoEncodeAccelerator::Config::EncoderType::kSoftware:
      return media::mojom::VideoEncodeAcceleratorConfig_EncoderType::kSoftware;
    case media::VideoEncodeAccelerator::Config::EncoderType::kNoPreference:
      return media::mojom::VideoEncodeAcceleratorConfig_EncoderType::
          kNoPreference;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_EncoderType,
                media::VideoEncodeAccelerator::Config::EncoderType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig_EncoderType input,
              media::VideoEncodeAccelerator::Config::EncoderType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_EncoderType::kHardware:
      *output = media::VideoEncodeAccelerator::Config::EncoderType::kHardware;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_EncoderType::kSoftware:
      *output = media::VideoEncodeAccelerator::Config::EncoderType::kSoftware;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_EncoderType::kNoPreference:
      *output =
          media::VideoEncodeAccelerator::Config::EncoderType::kNoPreference;
      return true;
  }
  NOTREACHED();
}

// static
media::mojom::VideoEncodeAcceleratorConfig_ContentType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_ContentType,
           media::VideoEncodeAccelerator::Config::ContentType>::
    ToMojom(media::VideoEncodeAccelerator::Config::ContentType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::ContentType::kDisplay:
      return media::mojom::VideoEncodeAcceleratorConfig_ContentType::kDisplay;
    case media::VideoEncodeAccelerator::Config::ContentType::kCamera:
      return media::mojom::VideoEncodeAcceleratorConfig_ContentType::kCamera;
  }
  NOTREACHED();
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_ContentType,
                media::VideoEncodeAccelerator::Config::ContentType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig_ContentType input,
              media::VideoEncodeAccelerator::Config::ContentType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_ContentType::kCamera:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kCamera;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_ContentType::kDisplay:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
      return true;
  }
  NOTREACHED();
}

// static
bool StructTraits<media::mojom::SpatialLayerDataView,
                  media::VideoEncodeAccelerator::Config::SpatialLayer>::
    Read(media::mojom::SpatialLayerDataView input,
         media::VideoEncodeAccelerator::Config::SpatialLayer* output) {
  output->width = input.width();
  output->height = input.height();
  output->bitrate_bps = input.bitrate_bps();
  output->framerate = input.framerate();
  output->max_qp = input.max_qp();
  output->num_of_temporal_layers = input.num_of_temporal_layers();
  return true;
}

// static
bool StructTraits<media::mojom::ConstantBitrateDataView, media::Bitrate>::Read(
    media::mojom::ConstantBitrateDataView input,
    media::Bitrate* output) {
  *output = media::Bitrate::ConstantBitrate(input.target_bps());
  return true;
}

// static
bool StructTraits<media::mojom::VariableBitrateDataView, media::Bitrate>::Read(
    media::mojom::VariableBitrateDataView input,
    media::Bitrate* output) {
  if (input.target_bps() > input.peak_bps())
    return false;
  if (input.peak_bps() == 0u)
    return false;
  *output =
      media::Bitrate::VariableBitrate(input.target_bps(), input.peak_bps());
  return true;
}

// static
bool StructTraits<media::mojom::ExternalBitrateDataView, media::Bitrate>::Read(
    media::mojom::ExternalBitrateDataView input,
    media::Bitrate* output) {
  *output = media::Bitrate::ExternalRateControl();
  return true;
}

// static
media::mojom::BitrateDataView::Tag
UnionTraits<media::mojom::BitrateDataView, media::Bitrate>::GetTag(
    const media::Bitrate& input) {
  switch (input.mode()) {
    case media::Bitrate::Mode::kConstant:
      return media::mojom::BitrateDataView::Tag::kConstant;
    case media::Bitrate::Mode::kVariable:
      return media::mojom::BitrateDataView::Tag::kVariable;
    case media::Bitrate::Mode::kExternal:
      return media::mojom::BitrateDataView::Tag::kExternal;
  }
  NOTREACHED();
}

// static
bool UnionTraits<media::mojom::BitrateDataView, media::Bitrate>::Read(
    media::mojom::BitrateDataView input,
    media::Bitrate* output) {
  switch (input.tag()) {
    case media::mojom::BitrateDataView::Tag::kConstant:
      return input.ReadConstant(output);
    case media::mojom::BitrateDataView::Tag::kVariable:
      return input.ReadVariable(output);
    case media::mojom::BitrateDataView::Tag::kExternal:
      return input.ReadExternal(output);
  }

  NOTREACHED();
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

  media::Bitrate bitrate;
  if (!input.ReadBitrate(&bitrate))
    return false;

  uint32_t framerate = input.framerate();

  std::optional<uint32_t> gop_length;
  if (input.has_gop_length())
    gop_length = input.gop_length();

  std::optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level())
    h264_output_level = input.h264_output_level();

  bool is_constrained_h264 = input.is_constrained_h264();

  media::VideoEncodeAccelerator::Config::StorageType storage_type;
  if (!input.ReadStorageType(&storage_type)) {
    return false;
  }

  media::VideoEncodeAccelerator::Config::ContentType content_type;
  if (!input.ReadContentType(&content_type))
    return false;

  uint8_t drop_frame_thresh_percentage = input.drop_frame_thresh_percentage();
  if (drop_frame_thresh_percentage > 100) {
    return false;
  }
  std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
      spatial_layers;
  if (!input.ReadSpatialLayers(&spatial_layers))
    return false;

  media::SVCInterLayerPredMode inter_layer_pred;
  if (!input.ReadInterLayerPred(&inter_layer_pred))
    return false;

  media::VideoEncodeAccelerator::Config::EncoderType required_encoder_type;
  if (!input.ReadRequiredEncoderType(&required_encoder_type))
    return false;

  struct CheckVEAConfig {
    // The variable declaration order must be the same as
    // VideoEncodeAccelerator::Config.
    media::VideoPixelFormat input_format;
    gfx::Size input_visible_size;
    media::VideoCodecProfile output_profile;
    media::Bitrate bitrate;
    uint32_t framerate;
    media::VideoEncodeAccelerator::Config::StorageType storage_type;
    media::VideoEncodeAccelerator::Config::ContentType content_type;
    std::optional<uint32_t> gop_length;
    std::optional<uint8_t> h264_output_level;
    bool is_constrained_h264;
    uint8_t drop_frame_thresh_percentage;
    std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
        spatial_layers;
    media::SVCInterLayerPredMode inter_layer_pred;
    bool require_low_delay;
    media::VideoEncodeAccelerator::Config::EncoderType required_encoder_type;
  };
  static_assert(
      sizeof(CheckVEAConfig) == sizeof(media::VideoEncodeAccelerator::Config),
      "Please apply removed/added values in VideoEncodeAccelerator::Config "
      "to the following copy and then remove/add the values in CheckVEAConfig");

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, bitrate, framerate,
      storage_type, content_type);

  output->gop_length = gop_length;
  output->h264_output_level = h264_output_level;
  output->is_constrained_h264 = is_constrained_h264;
  output->drop_frame_thresh_percentage = drop_frame_thresh_percentage;
  output->spatial_layers = spatial_layers;
  output->inter_layer_pred = inter_layer_pred;
  output->require_low_delay = input.require_low_delay();
  output->required_encoder_type = required_encoder_type;

  return true;
}

}  // namespace mojo
