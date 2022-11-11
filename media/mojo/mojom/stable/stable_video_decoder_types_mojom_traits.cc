// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"

#include "base/time/time.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/format_utils.h"
#include "media/gpu/buffer_validation.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_status.h"
#elif BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_status.h"
#endif

// This file contains a variety of conservative compile-time assertions that
// help us detect changes that may break the backward compatibility requirement
// of the StableVideoDecoder API. Specifically, we have static_asserts() that
// ensure the type of the media struct member is *exactly* the same as the
// corresponding mojo struct member. If this changes, we must be careful to
// validate ranges and avoid implicit conversions.
//
// If you need to make any changes to this file, please consult with
// chromeos-gfx-video@google.com first.

namespace mojo {

namespace {

media::stable::mojom::VideoFrameDataPtr MakeVideoFrameData(
    const media::VideoFrame* input) {
  if (input->metadata().end_of_stream) {
    return media::stable::mojom::VideoFrameData::NewEosData(
        media::stable::mojom::EosVideoFrameData::New());
  }

  CHECK_EQ(input->storage_type(), media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  CHECK(input->HasGpuMemoryBuffer());
  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
      input->GetGpuMemoryBuffer()->CloneHandle();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  CHECK_EQ(gpu_memory_buffer_handle.type, gfx::NATIVE_PIXMAP);
  CHECK(!gpu_memory_buffer_handle.native_pixmap_handle.planes.empty());
#else
  // We should not be trying to serialize a media::VideoFrame for the purposes
  // of this interface outside of Linux and Chrome OS.
  CHECK(false);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  return media::stable::mojom::VideoFrameData::NewGpuMemoryBufferData(
      media::stable::mojom::GpuMemoryBufferVideoFrameData::New(
          std::move(gpu_memory_buffer_handle)));
}

}  // namespace

// static
gfx::ColorSpace::PrimaryID
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::primaries(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::primaries_),
                   decltype(
                       media::stable::mojom::ColorSpace::primaries)>::value,
      "Unexpected type for gfx::ColorSpace::primaries_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.primaries_;
}

// static
gfx::ColorSpace::TransferID
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::transfer(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::transfer_),
                   decltype(media::stable::mojom::ColorSpace::transfer)>::value,
      "Unexpected type for gfx::ColorSpace::transfer_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.transfer_;
}

// static
gfx::ColorSpace::MatrixID
StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::matrix(
    const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::matrix_),
                   decltype(media::stable::mojom::ColorSpace::matrix)>::value,
      "Unexpected type for gfx::ColorSpace::matrix_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.matrix_;
}

// static
gfx::ColorSpace::RangeID
StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::range(
    const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::range_),
                   decltype(media::stable::mojom::ColorSpace::range)>::value,
      "Unexpected type for gfx::ColorSpace::range_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.range_;
}

// static
base::span<const float> StructTraits<
    media::stable::mojom::ColorSpaceDataView,
    gfx::ColorSpace>::custom_primary_matrix(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::custom_primary_matrix_),
                   float[9]>::value,
      "Unexpected type for gfx::ColorSpace::custom_primary_matrix_. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  return input.custom_primary_matrix_;
}

// static
base::span<const float>
StructTraits<media::stable::mojom::ColorSpaceDataView,
             gfx::ColorSpace>::transfer_params(const gfx::ColorSpace& input) {
  static_assert(
      std::is_same<decltype(::gfx::ColorSpace::transfer_params_),
                   float[7]>::value,
      "Unexpected type for gfx::ColorSpace::transfer_params_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.transfer_params_;
}

// static
bool StructTraits<media::stable::mojom::ColorSpaceDataView, gfx::ColorSpace>::
    Read(media::stable::mojom::ColorSpaceDataView input,
         gfx::ColorSpace* output) {
  if (!input.ReadPrimaries(&output->primaries_))
    return false;
  if (!input.ReadTransfer(&output->transfer_))
    return false;
  if (!input.ReadMatrix(&output->matrix_))
    return false;
  if (!input.ReadRange(&output->range_))
    return false;
  {
    base::span<float> matrix(output->custom_primary_matrix_);
    if (!input.ReadCustomPrimaryMatrix(&matrix))
      return false;
  }
  {
    base::span<float> transfer(output->transfer_params_);
    if (!input.ReadTransferParams(&transfer))
      return false;
  }
  return true;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::ColorVolumeMetadata>::
    primary_r(const gfx::ColorVolumeMetadata& input) {
  gfx::PointF primary_r(input.primaries.fRX, input.primaries.fRY);
  static_assert(
      std::is_same<decltype(primary_r),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_r)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::primary_r. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return primary_r;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::ColorVolumeMetadata>::
    primary_g(const gfx::ColorVolumeMetadata& input) {
  gfx::PointF primary_g(input.primaries.fGX, input.primaries.fGY);
  static_assert(
      std::is_same<decltype(primary_g),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_g)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::primary_g. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return primary_g;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::ColorVolumeMetadata>::
    primary_b(const gfx::ColorVolumeMetadata& input) {
  gfx::PointF primary_b(input.primaries.fBX, input.primaries.fBY);
  static_assert(
      std::is_same<decltype(primary_b),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_b)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::primary_b. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return primary_b;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::ColorVolumeMetadata>::
    white_point(const gfx::ColorVolumeMetadata& input) {
  gfx::PointF white_point(input.primaries.fWX, input.primaries.fWY);
  static_assert(
      std::is_same<decltype(white_point),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                white_point)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::white_point. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return white_point;
}

// static
float StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                   gfx::ColorVolumeMetadata>::
    luminance_max(const gfx::ColorVolumeMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::gfx::ColorVolumeMetadata::luminance_max),
          decltype(
              media::stable::mojom::ColorVolumeMetadata::luminance_max)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::luminance_max. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.luminance_max;
}

// static
float StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                   gfx::ColorVolumeMetadata>::
    luminance_min(const gfx::ColorVolumeMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::gfx::ColorVolumeMetadata::luminance_min),
          decltype(
              media::stable::mojom::ColorVolumeMetadata::luminance_min)>::value,
      "Unexpected type for gfx::ColorVolumeMetadata::luminance_min. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.luminance_min;
}

// static
bool StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                  gfx::ColorVolumeMetadata>::
    Read(media::stable::mojom::ColorVolumeMetadataDataView data,
         gfx::ColorVolumeMetadata* output) {
  output->luminance_max = data.luminance_max();
  output->luminance_min = data.luminance_min();
  gfx::PointF primary_r;
  if (!data.ReadPrimaryR(&primary_r))
    return false;
  gfx::PointF primary_g;
  if (!data.ReadPrimaryG(&primary_g))
    return false;
  gfx::PointF primary_b;
  if (!data.ReadPrimaryB(&primary_b))
    return false;
  gfx::PointF white_point;
  if (!data.ReadWhitePoint(&white_point))
    return false;
  output->primaries = {
      primary_r.x(), primary_r.y(), primary_g.x(),   primary_g.y(),
      primary_b.x(), primary_b.y(), white_point.x(), white_point.y(),
  };
  return true;
}

// static
base::TimeDelta StructTraits<media::stable::mojom::DecoderBufferDataView,
                             scoped_refptr<media::DecoderBuffer>>::
    timestamp(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->timestamp()),
                   decltype(
                       media::stable::mojom::DecoderBuffer::timestamp)>::value,
      "Unexpected type for media::DecoderBuffer::timestamp(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return input->timestamp();
  return base::TimeDelta();
}

// static
base::TimeDelta StructTraits<media::stable::mojom::DecoderBufferDataView,
                             scoped_refptr<media::DecoderBuffer>>::
    duration(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->duration()),
                   decltype(
                       media::stable::mojom::DecoderBuffer::duration)>::value,
      "Unexpected type for media::DecoderBuffer::duration(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return input->duration();
  return base::TimeDelta();
}

// static
bool StructTraits<media::stable::mojom::DecoderBufferDataView,
                  scoped_refptr<media::DecoderBuffer>>::
    is_end_of_stream(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<
          decltype(input->end_of_stream()),
          decltype(
              media::stable::mojom::DecoderBuffer::is_end_of_stream)>::value,
      "Unexpected type for media::DecoderBuffer::end_of_stream(). If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  return input->end_of_stream();
}

// static
uint32_t StructTraits<media::stable::mojom::DecoderBufferDataView,
                      scoped_refptr<media::DecoderBuffer>>::
    data_size(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->data_size()), size_t>::value,
      "Unexpected type for media::DecoderBuffer::data_size(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return base::checked_cast<uint32_t>(input->data_size());
  return 0u;
}

// static
bool StructTraits<media::stable::mojom::DecoderBufferDataView,
                  scoped_refptr<media::DecoderBuffer>>::
    is_key_frame(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<
          decltype(input->is_key_frame()),
          decltype(media::stable::mojom::DecoderBuffer::is_key_frame)>::value,
      "Unexpected type for media::DecoderBuffer::is_key_frame(). If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return input->is_key_frame();
  return false;
}

// static
std::vector<uint8_t> StructTraits<media::stable::mojom::DecoderBufferDataView,
                                  scoped_refptr<media::DecoderBuffer>>::
    side_data(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->side_data()), const uint8_t*>::value,
      "Unexpected type for media::DecoderBuffer::side_data(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  static_assert(std::is_same<decltype(input->side_data_size()), size_t>::value,
                "Unexpected type for media::DecoderBuffer::side_data_size(). "
                "If you need to change this assertion, please contact "
                "chromeos-gfx-video@google.com.");
  if (input->end_of_stream() || !input->side_data())
    return {};
  CHECK_GT(input->side_data_size(), 0u);
  // This copy is okay because the side data is expected to be small always.
  return std::vector<uint8_t>(input->side_data(),
                              input->side_data() + input->side_data_size());
}

// static
std::unique_ptr<media::DecryptConfig>
StructTraits<media::stable::mojom::DecoderBufferDataView,
             scoped_refptr<media::DecoderBuffer>>::
    decrypt_config(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(std::is_same<decltype(input->decrypt_config()),
                             const media::DecryptConfig*>::value,
                "Unexpected type for media::DecoderBuffer::decrypt_config(). "
                "If you need to change this assertion, please contact "
                "chromeos-gfx-video@google.com.");
  static_assert(
      std::is_same<
          decltype(input->decrypt_config()->Clone()),
          decltype(media::stable::mojom::DecoderBuffer::decrypt_config)>::value,
      "Unexpected type for media::DecoderBuffer::decrypt_config()->Clone(). If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  if (input->end_of_stream() || !input->decrypt_config())
    return nullptr;
  std::unique_ptr<media::DecryptConfig> decrypt_config =
      input->decrypt_config()->Clone();
  CHECK(!!decrypt_config);
  return decrypt_config;
}

// static
base::TimeDelta StructTraits<media::stable::mojom::DecoderBufferDataView,
                             scoped_refptr<media::DecoderBuffer>>::
    front_discard(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->discard_padding()),
                   const std::pair<base::TimeDelta, base::TimeDelta>&>::value,
      "Unexpected type for input->discard_padding(). If you need to change "
      "this assertion, please contact chromeos-gfx-video@google.com.");
  static_assert(
      std::is_same<
          decltype(input->discard_padding().first),
          decltype(media::stable::mojom::DecoderBuffer::front_discard)>::value,
      "Unexpected type for input->discard_padding().first. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return input->discard_padding().first;
  return base::TimeDelta();
}

// static
base::TimeDelta StructTraits<media::stable::mojom::DecoderBufferDataView,
                             scoped_refptr<media::DecoderBuffer>>::
    back_discard(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->discard_padding()),
                   const std::pair<base::TimeDelta, base::TimeDelta>&>::value,
      "Unexpected type for input->discard_padding(). If you need to change "
      "this assertion, please contact chromeos-gfx-video@google.com.");
  static_assert(
      std::is_same<
          decltype(input->discard_padding().second),
          decltype(media::stable::mojom::DecoderBuffer::back_discard)>::value,
      "Unexpected type for input->discard_padding().second. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return input->discard_padding().second;
  return base::TimeDelta();
}

// static
bool StructTraits<media::stable::mojom::DecoderBufferDataView,
                  scoped_refptr<media::DecoderBuffer>>::
    Read(media::stable::mojom::DecoderBufferDataView input,
         scoped_refptr<media::DecoderBuffer>* output) {
  if (input.is_end_of_stream()) {
    *output = media::DecoderBuffer::CreateEOSBuffer();
    return !!(*output);
  }
  auto decoder_buffer = base::MakeRefCounted<media::DecoderBuffer>(
      base::strict_cast<size_t>(input.data_size()));

  base::TimeDelta timestamp;
  if (!input.ReadTimestamp(&timestamp))
    return false;
  decoder_buffer->set_timestamp(timestamp);

  base::TimeDelta duration;
  if (!input.ReadTimestamp(&duration))
    return false;
  decoder_buffer->set_duration(duration);

  decoder_buffer->set_is_key_frame(input.is_key_frame());

  std::vector<uint8_t> side_data;
  if (!input.ReadSideData(&side_data))
    return false;
  if (!side_data.empty()) {
    decoder_buffer->CopySideDataFrom(side_data.data(), side_data.size());
  }

  std::unique_ptr<media::DecryptConfig> decrypt_config;
  if (!input.ReadDecryptConfig(&decrypt_config))
    return false;
  if (decrypt_config)
    decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  base::TimeDelta front_discard;
  if (!input.ReadFrontDiscard(&front_discard))
    return false;
  base::TimeDelta back_discard;
  if (!input.ReadBackDiscard(&back_discard))
    return false;
  static_assert(
      std::is_same<media::DecoderBuffer::DiscardPadding,
                   std::pair<base::TimeDelta, base::TimeDelta>>::value,
      "Unexpected type for media::DecoderBuffer::DiscardPadding. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  media::DecoderBuffer::DiscardPadding discard_padding(front_discard,
                                                       back_discard);
  decoder_buffer->set_discard_padding(discard_padding);

  *output = std::move(decoder_buffer);
  return true;
}

// static
media::EncryptionScheme
StructTraits<media::stable::mojom::DecryptConfigDataView,
             std::unique_ptr<media::DecryptConfig>>::
    encryption_scheme(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<
          decltype(input->encryption_scheme()),
          decltype(
              media::stable::mojom::DecryptConfig::encryption_scheme)>::value,
      "Unexpected type for media::DecryptConfig::encryption_scheme(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->encryption_scheme();
}

// static
const std::string& StructTraits<media::stable::mojom::DecryptConfigDataView,
                                std::unique_ptr<media::DecryptConfig>>::
    key_id(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<decltype(input->key_id()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::DecryptConfig::key_id)>::type>::
                       type>::value,
      "Unexpected type for media::DecryptConfig::key_id(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input->key_id();
}

// static
const std::string& StructTraits<media::stable::mojom::DecryptConfigDataView,
                                std::unique_ptr<media::DecryptConfig>>::
    iv(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<
          decltype(input->iv()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::DecryptConfig::iv)>::type>::type>::value,
      "Unexpected type for media::DecryptConfig::iv(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input->iv();
}

// static
const std::vector<media::SubsampleEntry>&
StructTraits<media::stable::mojom::DecryptConfigDataView,
             std::unique_ptr<media::DecryptConfig>>::
    subsamples(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<
          decltype(input->subsamples()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::DecryptConfig::subsamples)>::type>::type>::
          value,
      "Unexpected type for media::DecryptConfig::subsamples(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input->subsamples();
}

// static
const absl::optional<media::EncryptionPattern>&
StructTraits<media::stable::mojom::DecryptConfigDataView,
             std::unique_ptr<media::DecryptConfig>>::
    encryption_pattern(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<
          decltype(input->encryption_pattern()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::DecryptConfig::encryption_pattern)>::type>::
              type>::value,
      "Unexpected type for media::DecryptConfig::encryption_pattern(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->encryption_pattern();
}

// static
bool StructTraits<media::stable::mojom::DecryptConfigDataView,
                  std::unique_ptr<media::DecryptConfig>>::
    Read(media::stable::mojom::DecryptConfigDataView input,
         std::unique_ptr<media::DecryptConfig>* output) {
  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  std::string key_id;
  if (!input.ReadKeyId(&key_id))
    return false;

  std::string iv;
  if (!input.ReadIv(&iv))
    return false;

  std::vector<media::SubsampleEntry> subsamples;
  if (!input.ReadSubsamples(&subsamples))
    return false;

  absl::optional<media::EncryptionPattern> encryption_pattern;
  if (!input.ReadEncryptionPattern(&encryption_pattern))
    return false;

  *output = std::make_unique<media::DecryptConfig>(
      encryption_scheme, key_id, iv, subsamples, encryption_pattern);
  return true;
}

// static
uint32_t StructTraits<
    media::stable::mojom::HDRMetadataDataView,
    gfx::HDRMetadata>::max_content_light_level(const gfx::HDRMetadata& input) {
  static_assert(
      std::is_same<decltype(::gfx::HDRMetadata::max_content_light_level),
                   decltype(media::stable::mojom::HDRMetadata::
                                max_content_light_level)>::value,
      "Unexpected type for gfx::HDRMetadata::max_content_light_level. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.max_content_light_level;
}

// static
uint32_t
StructTraits<media::stable::mojom::HDRMetadataDataView, gfx::HDRMetadata>::
    max_frame_average_light_level(const gfx::HDRMetadata& input) {
  static_assert(
      std::is_same<decltype(::gfx::HDRMetadata::max_frame_average_light_level),
                   decltype(media::stable::mojom::HDRMetadata::
                                max_frame_average_light_level)>::value,
      "Unexpected type for gfx::HDRMetadata::max_frame_average_light_level. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.max_frame_average_light_level;
}

// static
const gfx::ColorVolumeMetadata& StructTraits<
    media::stable::mojom::HDRMetadataDataView,
    gfx::HDRMetadata>::color_volume_metadata(const gfx::HDRMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::gfx::HDRMetadata::color_volume_metadata),
          decltype(
              media::stable::mojom::HDRMetadata::color_volume_metadata)>::value,
      "Unexpected type for gfx::HDRMetadata::color_volume_metadata. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.color_volume_metadata;
}

// static
bool StructTraits<media::stable::mojom::HDRMetadataDataView, gfx::HDRMetadata>::
    Read(media::stable::mojom::HDRMetadataDataView data,
         gfx::HDRMetadata* output) {
  output->max_content_light_level = data.max_content_light_level();
  output->max_frame_average_light_level = data.max_frame_average_light_level();
  if (!data.ReadColorVolumeMetadata(&output->color_volume_metadata))
    return false;
  return true;
}

// static
int32_t
StructTraits<media::stable::mojom::MediaLogRecordDataView,
             media::MediaLogRecord>::id(const media::MediaLogRecord& input) {
  static_assert(
      std::is_same<decltype(::media::MediaLogRecord::id),
                   decltype(media::stable::mojom::MediaLogRecord::id)>::value,
      "Unexpected type for media::MediaLogRecord::id. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.id;
}

// static
media::MediaLogRecord::Type
StructTraits<media::stable::mojom::MediaLogRecordDataView,
             media::MediaLogRecord>::type(const media::MediaLogRecord& input) {
  static_assert(
      std::is_same<decltype(::media::MediaLogRecord::type),
                   decltype(media::stable::mojom::MediaLogRecord::type)>::value,
      "Unexpected type for media::MediaLogRecord::type. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.type;
}

// static
const base::Value::Dict& StructTraits<
    media::stable::mojom::MediaLogRecordDataView,
    media::MediaLogRecord>::params(const media::MediaLogRecord& input) {
  static_assert(std::is_same<decltype(::media::MediaLogRecord::params),
                             base::Value::Dict>::value,
                "Unexpected type for media::MediaLogRecord::params. If you "
                "need to change this assertion, please contact "
                "chromeos-gfx-video@google.com.");

  return input.params;
}

// static
base::TimeTicks
StructTraits<media::stable::mojom::MediaLogRecordDataView,
             media::MediaLogRecord>::time(const media::MediaLogRecord& input) {
  static_assert(
      std::is_same<decltype(::media::MediaLogRecord::time),
                   decltype(media::stable::mojom::MediaLogRecord::time)>::value,
      "Unexpected type for media::MediaLogRecord::time. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.time;
}

// static
bool StructTraits<media::stable::mojom::MediaLogRecordDataView,
                  media::MediaLogRecord>::
    Read(media::stable::mojom::MediaLogRecordDataView data,
         media::MediaLogRecord* output) {
  output->id = data.id();

  if (!data.ReadType(&output->type))
    return false;

  if (!data.ReadParams(&output->params))
    return false;

  if (!data.ReadTime(&output->time))
    return false;

  return true;
}

// static
const gfx::GpuMemoryBufferId& StructTraits<
    media::stable::mojom::NativeGpuMemoryBufferHandleDataView,
    gfx::GpuMemoryBufferHandle>::id(const gfx::GpuMemoryBufferHandle& input) {
  static_assert(
      std::is_same<
          decltype(::gfx::GpuMemoryBufferHandle::id),
          decltype(
              media::stable::mojom::NativeGpuMemoryBufferHandle::id)>::value,
      "Unexpected type for gfx::GpuMemoryBufferHandle::id. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.id;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// static
gfx::NativePixmapHandle StructTraits<
    media::stable::mojom::NativeGpuMemoryBufferHandleDataView,
    gfx::GpuMemoryBufferHandle>::platform_handle(gfx::GpuMemoryBufferHandle&
                                                     input) {
  CHECK_EQ(input.type, gfx::NATIVE_PIXMAP);
  return std::move(input.native_pixmap_handle);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// static
bool StructTraits<media::stable::mojom::NativeGpuMemoryBufferHandleDataView,
                  gfx::GpuMemoryBufferHandle>::
    Read(media::stable::mojom::NativeGpuMemoryBufferHandleDataView data,
         gfx::GpuMemoryBufferHandle* output) {
  if (!data.ReadId(&output->id))
    return false;

  output->type = gfx::NATIVE_PIXMAP;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (!data.ReadPlatformHandle(&output->native_pixmap_handle))
    return false;
  return true;
#else
  // We should not be trying to de-serialize a gfx::GpuMemoryBufferHandle for
  // the purposes of this interface outside of Linux and Chrome OS.
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

// static
media::stable::mojom::StatusCode StructTraits<
    media::stable::mojom::StatusDataDataView,
    media::internal::StatusData>::code(const media::internal::StatusData&
                                           input) {
  static_assert(
      std::is_same_v<decltype(::media::internal::StatusData::code), uint16_t>,
      "Unexpected type for media::internal::StatusData::code. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  // TODO(b/215438024): enforce that this check implies that the input.code
  // really corresponds to media::DecoderStatusTraits::Codes.
  CHECK(input.group == media::DecoderStatusTraits::Group());

  static_assert(
      std::is_same_v<std::underlying_type_t<media::DecoderStatusTraits::Codes>,
                     uint16_t>,
      "Unexpected underlying type for media::DecoderStatusTraits::Codes. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  if (input.code ==
      static_cast<uint16_t>(media::DecoderStatusTraits::Codes::kOk)) {
    return media::stable::mojom::StatusCode::kOk;
  } else if (input.code == static_cast<uint16_t>(
                               media::DecoderStatusTraits::Codes::kAborted)) {
    return media::stable::mojom::StatusCode::kAborted;
  }
  return media::stable::mojom::StatusCode::kError;
}

// static
std::string StructTraits<media::stable::mojom::StatusDataDataView,
                         media::internal::StatusData>::
    group(const media::internal::StatusData& input) {
  static_assert(
      std::is_same<decltype(::media::internal::StatusData::group),
                   decltype(media::stable::mojom::StatusData::group)>::value,
      "Unexpected type for media::internal::StatusData::group. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  CHECK(input.group == media::DecoderStatusTraits::Group());

  return input.group;
}

// static
std::string StructTraits<media::stable::mojom::StatusDataDataView,
                         media::internal::StatusData>::
    message(const media::internal::StatusData& input) {
  static_assert(
      std::is_same<decltype(::media::internal::StatusData::message),
                   decltype(media::stable::mojom::StatusData::message)>::value,
      "Unexpected type for media::internal::StatusData::message. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.message;
}

// static
base::span<const base::Value> StructTraits<
    media::stable::mojom::StatusDataDataView,
    media::internal::StatusData>::frames(const media::internal::StatusData&
                                             input) {
  static_assert(
      std::is_same<decltype(::media::internal::StatusData::frames),
                   decltype(media::stable::mojom::StatusData::frames)>::value,
      "Unexpected type for media::internal::StatusData::frames. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.frames;
}

// static
absl::optional<media::internal::StatusData> StructTraits<
    media::stable::mojom::StatusDataDataView,
    media::internal::StatusData>::cause(const media::internal::StatusData&
                                            input) {
  static_assert(
      std::is_same<decltype(*input.cause),
                   std::add_lvalue_reference<decltype(
                       media::stable::mojom::StatusData::cause)::value_type>::
                       type>::value,
      "Unexpected type for media::internal::StatusData::cause. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  static_assert(
      std::is_same_v<std::underlying_type_t<media::DecoderStatusTraits::Codes>,
                     media::StatusCodeType>,
      "Unexpected underlying type for media::DecoderStatusTraits::Codes. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

#if BUILDFLAG(USE_VAAPI)
  static_assert(
      std::is_same_v<std::underlying_type_t<media::VaapiStatusTraits::Codes>,
                     uint16_t>,
      "Unexpected underlying type for media::VaapiStatusTraits::Codes. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  static_assert(
      static_cast<uint16_t>(media::VaapiStatusTraits::Codes::kOk) == 0u,
      "Unexpected value for media::VaapiStatusTraits::Codes::kOk. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
#elif BUILDFLAG(USE_V4L2_CODEC)
  static_assert(
      std::is_same_v<std::underlying_type_t<media::V4L2StatusTraits::Codes>,
                     uint16_t>,
      "Unexpected underlying type for media::V4L2StatusTraits::Codes. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
  static_assert(
      static_cast<uint16_t>(media::V4L2StatusTraits::Codes::kOk) == 0u,
      "Unexpected value for media::V4L2StatusTraits::Codes::kOk. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");
#endif

  if (input.cause) {
    media::internal::StatusData output_cause(*input.cause);
    static_assert(
        std::is_same_v<decltype(output_cause.code), uint16_t>,
        "Unexpected type for output_cause.code. If you need to change this "
        "assertion, please contact chromeos-gfx-video@google.com.");

    static_assert(
        std::is_same_v<decltype(output_cause.code), media::StatusCodeType>,
        "Unexpected type for output_cause.code. If you need to "
        "change this assertion, please contact chromeos-gfx-video@google.com.");

    // TODO(b/215438024): enforce that these checks imply that the
    // output_cause.code really corresponds to media::VaapiStatusTraits::Codes
    // or media::V4L2StatusTraits::Codes.
#if BUILDFLAG(USE_VAAPI)
    CHECK(output_cause.group == media::VaapiStatusTraits::Group());
#elif BUILDFLAG(USE_V4L2_CODEC)
    CHECK(output_cause.group == media::V4L2StatusTraits::Group());
#else
    // TODO(b/217970098): allow building the VaapiStatusTraits and
    // V4L2StatusTraits without USE_VAAPI/USE_V4L2_CODEC so these guards could
    // be removed.
    CHECK(false);
#endif
    output_cause.code = static_cast<media::StatusCodeType>(
        output_cause.code ? media::DecoderStatusTraits::Codes::kFailed
                          : media::DecoderStatusTraits::Codes::kOk);
    output_cause.group = std::string(media::DecoderStatusTraits::Group());
    return output_cause;
  }
  return absl::nullopt;
}

// static
const base::Value& StructTraits<media::stable::mojom::StatusDataDataView,
                                media::internal::StatusData>::
    data(const media::internal::StatusData& input) {
  static_assert(
      std::is_same<decltype(input.data.Clone()),
                   decltype(media::stable::mojom::StatusData::data)>::value,
      "Unexpected type for media::internal::StatusData::data::Clone(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.data;
}

// static
bool StructTraits<media::stable::mojom::StatusDataDataView,
                  media::internal::StatusData>::
    Read(media::stable::mojom::StatusDataDataView data,
         media::internal::StatusData* output) {
  static_assert(
      std::is_same<decltype(output->code), media::StatusCodeType>::value,
      "Unexpected type for media::internal::StatusData::code. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  static_assert(
      std::is_same_v<std::underlying_type_t<media::DecoderStatusTraits::Codes>,
                     media::StatusCodeType>,
      "Unexpected underlying type for media::DecoderStatusTraits::Codes. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  switch (data.code()) {
    case media::stable::mojom::StatusCode::kOk:
      output->code = static_cast<media::StatusCodeType>(
          media::DecoderStatusTraits::Codes::kOk);
      break;
    case media::stable::mojom::StatusCode::kAborted:
      output->code = static_cast<media::StatusCodeType>(
          media::DecoderStatusTraits::Codes::kAborted);
      break;
    case media::stable::mojom::StatusCode::kError:
      output->code = static_cast<media::StatusCodeType>(
          media::DecoderStatusTraits::Codes::kFailed);
      break;
    default:
      return false;
  }

  // TODO(b/215438024): the group will always be the one that corresponds
  // to the DecoderStatus::Codes so it does not have to be sent over IPC.
  // Remove the group field from the media::stable::mojom::StatusData
  // structure.
  output->group = std::string(media::DecoderStatusTraits::Group());

  if (!data.ReadMessage(&output->message))
    return false;

  if (!data.ReadFrames(&output->frames))
    return false;

  if (!data.ReadData(&output->data))
    return false;

  absl::optional<media::internal::StatusData> cause;
  if (!data.ReadCause(&cause))
    return false;

  if (cause.has_value()) {
    output->cause =
        std::make_unique<media::internal::StatusData>(std::move(*cause));
  }

  return true;
}

// static
mojo::OptionalAsPointer<const media::internal::StatusData> StructTraits<
    media::stable::mojom::StatusDataView,
    media::DecoderStatus>::internal(const media::DecoderStatus& input) {
  static_assert(
      std::is_same<decltype(*input.data_),
                   std::add_lvalue_reference<decltype(
                       media::stable::mojom::Status::internal)::value_type>::
                       type>::value,
      "Unexpected type for media::DecoderStatus::data_. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  CHECK(input.data_ || input.is_ok());

  return MakeOptionalAsPointer(input.data_.get());
}

// static
bool StructTraits<media::stable::mojom::StatusDataView, media::DecoderStatus>::
    Read(media::stable::mojom::StatusDataView data,
         media::DecoderStatus* output) {
  absl::optional<media::internal::StatusData> internal;
  if (!data.ReadInternal(&internal))
    return false;
  if (internal) {
    output->data_ = internal->copy();
    return !!output->data_;
  }
  *output = media::DecoderStatus(media::OkStatus());
  return true;
}

// static
media::VideoCodecProfile
StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
             media::SupportedVideoDecoderConfig>::
    profile_min(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(::media::SupportedVideoDecoderConfig::profile_min),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                profile_min)>::value,
      "Unexpected type for media::SupportedVideoDecoderConfig::profile_min. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.profile_min;
}

// static
media::VideoCodecProfile
StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
             media::SupportedVideoDecoderConfig>::
    profile_max(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(::media::SupportedVideoDecoderConfig::profile_max),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                profile_max)>::value,
      "Unexpected type for media::SupportedVideoDecoderConfig::profile_max. If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.profile_max;
}

// static
const gfx::Size&
StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
             media::SupportedVideoDecoderConfig>::
    coded_size_min(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(
                       ::media::SupportedVideoDecoderConfig::coded_size_min),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                coded_size_min)>::value,
      "Unexpected type for media::SupportedVideoDecoderConfig::coded_size_min. "
      "If you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.coded_size_min;
}

// static
const gfx::Size&
StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
             media::SupportedVideoDecoderConfig>::
    coded_size_max(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(
                       ::media::SupportedVideoDecoderConfig::coded_size_max),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                coded_size_max)>::value,
      "Unexpected type for media::SupportedVideoDecoderConfig::coded_size_max. "
      "If you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.coded_size_max;
}

// static
bool StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
                  media::SupportedVideoDecoderConfig>::
    allow_encrypted(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(
                       ::media::SupportedVideoDecoderConfig::allow_encrypted),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                allow_encrypted)>::value,
      "Unexpected type for "
      "media::SupportedVideoDecoderConfig::allow_encrypted. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.allow_encrypted;
}

// static
bool StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
                  media::SupportedVideoDecoderConfig>::
    require_encrypted(const media::SupportedVideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(
                       ::media::SupportedVideoDecoderConfig::require_encrypted),
                   decltype(media::stable::mojom::SupportedVideoDecoderConfig::
                                require_encrypted)>::value,
      "Unexpected type for "
      "media::SupportedVideoDecoderConfig::require_encrypted. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.require_encrypted;
}

// static
bool StructTraits<media::stable::mojom::SupportedVideoDecoderConfigDataView,
                  media::SupportedVideoDecoderConfig>::
    Read(media::stable::mojom::SupportedVideoDecoderConfigDataView input,
         media::SupportedVideoDecoderConfig* output) {
  if (!input.ReadProfileMin(&output->profile_min))
    return false;

  if (!input.ReadProfileMax(&output->profile_max))
    return false;

  if (!input.ReadCodedSizeMin(&output->coded_size_min))
    return false;

  if (!input.ReadCodedSizeMax(&output->coded_size_max))
    return false;

  output->allow_encrypted = input.allow_encrypted();
  output->require_encrypted = input.require_encrypted();

  return true;
}

// static
uint32_t StructTraits<
    media::stable::mojom::SubsampleEntryDataView,
    media::SubsampleEntry>::clear_bytes(const ::media::SubsampleEntry& input) {
  static_assert(
      std::is_same<
          decltype(::media::SubsampleEntry::clear_bytes),
          decltype(media::stable::mojom::SubsampleEntry::clear_bytes)>::value,
      "Unexpected type for media::SubsampleEntry::clear_bytes. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.clear_bytes;
}

// static
uint32_t StructTraits<
    media::stable::mojom::SubsampleEntryDataView,
    media::SubsampleEntry>::cypher_bytes(const ::media::SubsampleEntry& input) {
  static_assert(
      std::is_same<
          decltype(::media::SubsampleEntry::cypher_bytes),
          decltype(media::stable::mojom::SubsampleEntry::cypher_bytes)>::value,
      "Unexpected type for media::SubsampleEntry::cypher_bytes. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.cypher_bytes;
}

// static
bool StructTraits<media::stable::mojom::SubsampleEntryDataView,
                  media::SubsampleEntry>::
    Read(media::stable::mojom::SubsampleEntryDataView input,
         media::SubsampleEntry* output) {
  *output = media::SubsampleEntry(input.clear_bytes(), input.cypher_bytes());
  return true;
}

// static
media::VideoCodec StructTraits<
    media::stable::mojom::VideoDecoderConfigDataView,
    media::VideoDecoderConfig>::codec(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(input.codec()),
                   decltype(
                       media::stable::mojom::VideoDecoderConfig::codec)>::value,
      "Unexpected type for media::VideoDecoderConfig::codec(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.codec();
}

// static
media::VideoCodecProfile StructTraits<
    media::stable::mojom::VideoDecoderConfigDataView,
    media::VideoDecoderConfig>::profile(const media::VideoDecoderConfig&
                                            input) {
  static_assert(
      std::is_same<
          decltype(input.profile()),
          decltype(media::stable::mojom::VideoDecoderConfig::profile)>::value,
      "Unexpected type for media::VideoDecoderConfig::profile(). If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.profile();
}

// static
bool StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
                  media::VideoDecoderConfig>::
    has_alpha(const media::VideoDecoderConfig& input) {
  static_assert(std::is_same<decltype(input.alpha_mode()),
                             media::VideoDecoderConfig::AlphaMode>::value,
                "Unexpected type for media::VideoDecoderConfig::alpha_mode(). "
                "If you need to change this assertion, please contact "
                "chromeos-gfx-video@google.com.");

  // This is deliberately written as a switch so that we get alerted when
  // someone makes changes to media::VideoDecoderConfig::AlphaMode.
  switch (input.alpha_mode()) {
    case media::VideoDecoderConfig::AlphaMode::kHasAlpha:
      return true;
    case media::VideoDecoderConfig::AlphaMode::kIsOpaque:
      return false;
  }

  NOTREACHED();
  return false;
}

// static
const gfx::Size& StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
                              media::VideoDecoderConfig>::
    coded_size(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(input.coded_size()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoDecoderConfig::coded_size)>::
                                                 type>::type>::value,
      "Unexpected type for media::VideoDecoderConfig::coded_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.coded_size();
}

// static
const gfx::Rect& StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
                              media::VideoDecoderConfig>::
    visible_rect(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<
          decltype(input.visible_rect()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::VideoDecoderConfig::visible_rect)>::type>::
              type>::value,
      "Unexpected type for media::VideoDecoderConfig::visible_rect(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.visible_rect();
}

// static
const gfx::Size& StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
                              media::VideoDecoderConfig>::
    natural_size(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<
          decltype(input.natural_size()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::VideoDecoderConfig::natural_size)>::type>::
              type>::value,
      "Unexpected type for media::VideoDecoderConfig::natural_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.natural_size();
}

// static
const std::vector<uint8_t>& StructTraits<
    media::stable::mojom::VideoDecoderConfigDataView,
    media::VideoDecoderConfig>::extra_data(const media::VideoDecoderConfig&
                                               input) {
  static_assert(
      std::is_same<decltype(input.extra_data()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoDecoderConfig::extra_data)>::
                                                 type>::type>::value,
      "Unexpected type for media::VideoDecoderConfig::extra_data(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.extra_data();
}

// static
media::EncryptionScheme
StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
             media::VideoDecoderConfig>::
    encryption_scheme(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(input.encryption_scheme()),
                   decltype(media::stable::mojom::VideoDecoderConfig::
                                encryption_scheme)>::value,
      "Unexpected type for media::VideoDecoderConfig::encryption_scheme(). If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.encryption_scheme();
}

// static
const gfx::ColorSpace
StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
             media::VideoDecoderConfig>::
    color_space_info(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(input.color_space_info()),
                   const media::VideoColorSpace&>::value,
      "Unexpected type for media::VideoDecoderConfig::color_space_info(). If "
      "you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.color_space_info().ToGfxColorSpace();
}

// static
const absl::optional<gfx::HDRMetadata>& StructTraits<
    media::stable::mojom::VideoDecoderConfigDataView,
    media::VideoDecoderConfig>::hdr_metadata(const media::VideoDecoderConfig&
                                                 input) {
  static_assert(
      std::is_same<
          decltype(input.hdr_metadata()),
          std::add_lvalue_reference<std::add_const<decltype(
              media::stable::mojom::VideoDecoderConfig::hdr_metadata)>::type>::
              type>::value,
      "Unexpected type for media::VideoDecoderConfig::hdr_metadata(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.hdr_metadata();
}

// static
uint32_t StructTraits<
    media::stable::mojom::VideoDecoderConfigDataView,
    media::VideoDecoderConfig>::level(const media::VideoDecoderConfig& input) {
  static_assert(
      std::is_same<decltype(input.level()),
                   decltype(
                       media::stable::mojom::VideoDecoderConfig::level)>::value,
      "Unexpected type for media::VideoDecoderConfig::level(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.level();
}

// static
bool StructTraits<media::stable::mojom::VideoDecoderConfigDataView,
                  media::VideoDecoderConfig>::
    Read(media::stable::mojom::VideoDecoderConfigDataView input,
         media::VideoDecoderConfig* output) {
  media::VideoCodec codec;
  if (!input.ReadCodec(&codec))
    return false;

  media::VideoCodecProfile profile;
  if (!input.ReadProfile(&profile))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  std::vector<uint8_t> extra_data;
  if (!input.ReadExtraData(&extra_data))
    return false;

  media::EncryptionScheme encryption_scheme;
  if (!input.ReadEncryptionScheme(&encryption_scheme))
    return false;

  gfx::ColorSpace color_space;
  if (!input.ReadColorSpaceInfo(&color_space))
    return false;

  absl::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;

  output->Initialize(codec, profile,
                     input.has_alpha()
                         ? media::VideoDecoderConfig::AlphaMode::kHasAlpha
                         : media::VideoDecoderConfig::AlphaMode::kIsOpaque,
                     media::VideoColorSpace::FromGfxColorSpace(color_space),
                     media::VideoTransformation(), coded_size, visible_rect,
                     natural_size, extra_data, encryption_scheme);

  if (hdr_metadata)
    output->set_hdr_metadata(hdr_metadata.value());

  output->set_level(input.level());

  if (!output->IsValidConfig())
    return false;

  return true;
}

// static
media::VideoPixelFormat StructTraits<media::stable::mojom::VideoFrameDataView,
                                     scoped_refptr<media::VideoFrame>>::
    format(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->format()),
                   decltype(media::stable::mojom::VideoFrame::format)>::value,
      "Unexpected type for media::VideoFrame::format(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->format();
}

// static
const gfx::Size& StructTraits<media::stable::mojom::VideoFrameDataView,
                              scoped_refptr<media::VideoFrame>>::
    coded_size(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->coded_size()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoFrame::coded_size)>::type>::
                       type>::value,
      "Unexpected type for media::VideoFrame::coded_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->coded_size();
}

// static
const gfx::Rect& StructTraits<media::stable::mojom::VideoFrameDataView,
                              scoped_refptr<media::VideoFrame>>::
    visible_rect(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->visible_rect()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoFrame::visible_rect)>::type>::
                       type>::value,
      "Unexpected type for media::VideoFrame::visible_rect(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->visible_rect();
}

// static
const gfx::Size& StructTraits<media::stable::mojom::VideoFrameDataView,
                              scoped_refptr<media::VideoFrame>>::
    natural_size(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->natural_size()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoFrame::natural_size)>::type>::
                       type>::value,
      "Unexpected type for media::VideoFrame::natural_size(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->natural_size();
}

// static
base::TimeDelta StructTraits<media::stable::mojom::VideoFrameDataView,
                             scoped_refptr<media::VideoFrame>>::
    timestamp(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->timestamp()),
                   decltype(
                       media::stable::mojom::VideoFrame::timestamp)>::value,
      "Unexpected type for media::VideoFrame::timestamp(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->timestamp();
}

// static
gfx::ColorSpace StructTraits<media::stable::mojom::VideoFrameDataView,
                             scoped_refptr<media::VideoFrame>>::
    color_space(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->ColorSpace()),
                   decltype(
                       media::stable::mojom::VideoFrame::color_space)>::value,
      "Unexpected type for media::VideoFrame::ColorSpace(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->ColorSpace();
}

// static
const absl::optional<gfx::HDRMetadata>&
StructTraits<media::stable::mojom::VideoFrameDataView,
             scoped_refptr<media::VideoFrame>>::
    hdr_metadata(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<decltype(input->hdr_metadata()),
                   std::add_lvalue_reference<std::add_const<decltype(
                       media::stable::mojom::VideoFrame::hdr_metadata)>::type>::
                       type>::value,
      "Unexpected type for media::VideoFrame::hdr_metadata(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->hdr_metadata();
}

// static
media::stable::mojom::VideoFrameDataPtr
StructTraits<media::stable::mojom::VideoFrameDataView,
             scoped_refptr<media::VideoFrame>>::
    data(const scoped_refptr<media::VideoFrame>& input) {
  return MakeVideoFrameData(input.get());
}

// static
const media::VideoFrameMetadata&
StructTraits<media::stable::mojom::VideoFrameDataView,
             scoped_refptr<media::VideoFrame>>::
    metadata(const scoped_refptr<media::VideoFrame>& input) {
  static_assert(
      std::is_same<
          decltype(input->metadata()),
          std::add_lvalue_reference<decltype(
              media::stable::mojom::VideoFrame::metadata)>::type>::value,
      "Unexpected type for media::VideoFrame::metadata(). If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input->metadata();
}

// static
bool StructTraits<media::stable::mojom::VideoFrameDataView,
                  scoped_refptr<media::VideoFrame>>::
    Read(media::stable::mojom::VideoFrameDataView input,
         scoped_refptr<media::VideoFrame>* output) {
  // View of the |data| member of the input media::stable::mojom::VideoFrame.
  media::stable::mojom::VideoFrameDataDataView data;
  input.GetDataDataView(&data);

  if (data.is_eos_data()) {
    *output = media::VideoFrame::CreateEOSFrame();
    return !!*output;
  }

  media::VideoPixelFormat format;
  if (!input.ReadFormat(&format))
    return false;

  gfx::Size coded_size;
  if (!input.ReadCodedSize(&coded_size))
    return false;

  gfx::Rect visible_rect;
  if (!input.ReadVisibleRect(&visible_rect))
    return false;

  if (!gfx::Rect(coded_size).Contains(visible_rect))
    return false;

  gfx::Size natural_size;
  if (!input.ReadNaturalSize(&natural_size))
    return false;

  base::TimeDelta timestamp;
  if (!input.ReadTimestamp(&timestamp))
    return false;

  scoped_refptr<media::VideoFrame> frame;
  if (data.is_gpu_memory_buffer_data()) {
    media::stable::mojom::GpuMemoryBufferVideoFrameDataDataView
        gpu_memory_buffer_data;
    data.GetGpuMemoryBufferDataDataView(&gpu_memory_buffer_data);

    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
    if (!gpu_memory_buffer_data.ReadGpuMemoryBufferHandle(
            &gpu_memory_buffer_handle)) {
      return false;
    }

    if (!media::VerifyGpuMemoryBufferHandle(format, coded_size,
                                            gpu_memory_buffer_handle)) {
      return false;
    }

    absl::optional<gfx::BufferFormat> buffer_format =
        VideoPixelFormatToGfxBufferFormat(format);
    if (!buffer_format)
      return false;

    gpu::GpuMemoryBufferSupport support;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
        support.CreateGpuMemoryBufferImplFromHandle(
            std::move(gpu_memory_buffer_handle), coded_size, *buffer_format,
            gfx::BufferUsage::SCANOUT_VDA_WRITE, base::NullCallback());
    if (!gpu_memory_buffer)
      return false;

    gpu::MailboxHolder dummy_mailbox[media::VideoFrame::kMaxPlanes];
    frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        visible_rect, natural_size, std::move(gpu_memory_buffer), dummy_mailbox,
        base::NullCallback(), timestamp);

  } else {
    NOTREACHED();
    return false;
  }

  if (!frame)
    return false;

  media::VideoFrameMetadata metadata;
  if (!input.ReadMetadata(&metadata))
    return false;

  frame->set_metadata(metadata);

  gfx::ColorSpace color_space;
  if (!input.ReadColorSpace(&color_space))
    return false;
  frame->set_color_space(color_space);

  absl::optional<gfx::HDRMetadata> hdr_metadata;
  if (!input.ReadHdrMetadata(&hdr_metadata))
    return false;
  frame->set_hdr_metadata(std::move(hdr_metadata));

  *output = std::move(frame);
  return true;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    allow_overlay(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::media::VideoFrameMetadata::allow_overlay),
          decltype(
              media::stable::mojom::VideoFrameMetadata::allow_overlay)>::value,
      "Unexpected type for media::VideoFrameMetadata::allow_overlay. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.allow_overlay;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    end_of_stream(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::media::VideoFrameMetadata::end_of_stream),
          decltype(
              media::stable::mojom::VideoFrameMetadata::end_of_stream)>::value,
      "Unexpected type for media::VideoFrameMetadata::end_of_stream. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.end_of_stream;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    read_lock_fences_enabled(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<decltype(
                       ::media::VideoFrameMetadata::read_lock_fences_enabled),
                   decltype(media::stable::mojom::VideoFrameMetadata::
                                read_lock_fences_enabled)>::value,
      "Unexpected type for "
      "media::VideoFrameMetadata::read_lock_fences_enabled. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return input.read_lock_fences_enabled;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    protected_video(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<decltype(::media::VideoFrameMetadata::protected_video),
                   decltype(media::stable::mojom::VideoFrameMetadata::
                                protected_video)>::value,
      "Unexpected type for media::VideoFrameMetadata::protected_video. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.protected_video;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    hw_protected(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<
          decltype(::media::VideoFrameMetadata::hw_protected),
          decltype(
              media::stable::mojom::VideoFrameMetadata::hw_protected)>::value,
      "Unexpected type for media::VideoFrameMetadata::hw_protected. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.hw_protected;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    power_efficient(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<decltype(::media::VideoFrameMetadata::power_efficient),
                   decltype(media::stable::mojom::VideoFrameMetadata::
                                power_efficient)>::value,
      "Unexpected type for media::VideoFrameMetadata::power_efficient. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.power_efficient;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    Read(media::stable::mojom::VideoFrameMetadataDataView input,
         media::VideoFrameMetadata* output) {
  output->allow_overlay = input.allow_overlay();
  output->end_of_stream = input.end_of_stream();
  output->read_lock_fences_enabled = input.read_lock_fences_enabled();
  output->protected_video = input.protected_video();
  output->hw_protected = input.hw_protected();
  output->power_efficient = input.power_efficient();

  return true;
}

}  // namespace mojo
