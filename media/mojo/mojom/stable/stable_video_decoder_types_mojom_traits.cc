// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/stable/stable_video_decoder_types_mojom_traits.h"

#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
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
                         gfx::HdrMetadataSmpteSt2086>::
    primary_r(const gfx::HdrMetadataSmpteSt2086& input) {
  gfx::PointF primary_r(input.primaries.fRX, input.primaries.fRY);
  static_assert(
      std::is_same<decltype(primary_r),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_r)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::primary_r. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return primary_r;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::HdrMetadataSmpteSt2086>::
    primary_g(const gfx::HdrMetadataSmpteSt2086& input) {
  gfx::PointF primary_g(input.primaries.fGX, input.primaries.fGY);
  static_assert(
      std::is_same<decltype(primary_g),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_g)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::primary_g. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return primary_g;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::HdrMetadataSmpteSt2086>::
    primary_b(const gfx::HdrMetadataSmpteSt2086& input) {
  gfx::PointF primary_b(input.primaries.fBX, input.primaries.fBY);
  static_assert(
      std::is_same<decltype(primary_b),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                primary_b)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::primary_b. If you need "
      "to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return primary_b;
}

// static
gfx::PointF StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                         gfx::HdrMetadataSmpteSt2086>::
    white_point(const gfx::HdrMetadataSmpteSt2086& input) {
  gfx::PointF white_point(input.primaries.fWX, input.primaries.fWY);
  static_assert(
      std::is_same<decltype(white_point),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                white_point)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::white_point. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return white_point;
}

// static
float StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                   gfx::HdrMetadataSmpteSt2086>::
    luminance_max(const gfx::HdrMetadataSmpteSt2086& input) {
  static_assert(
      std::is_same<decltype(::gfx::HdrMetadataSmpteSt2086::luminance_max),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                luminance_max)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::luminance_max. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.luminance_max;
}

// static
float StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                   gfx::HdrMetadataSmpteSt2086>::
    luminance_min(const gfx::HdrMetadataSmpteSt2086& input) {
  static_assert(
      std::is_same<decltype(::gfx::HdrMetadataSmpteSt2086::luminance_min),
                   decltype(media::stable::mojom::ColorVolumeMetadata::
                                luminance_min)>::value,
      "Unexpected type for gfx::HdrMetadataSmpteSt2086::luminance_min. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.luminance_min;
}

// static
bool StructTraits<media::stable::mojom::ColorVolumeMetadataDataView,
                  gfx::HdrMetadataSmpteSt2086>::
    Read(media::stable::mojom::ColorVolumeMetadataDataView data,
         gfx::HdrMetadataSmpteSt2086* output) {
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
      std::is_same<decltype(input->size()), size_t>::value,
      "Unexpected type for media::DecoderBuffer::size(). If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  if (!input->end_of_stream())
    return base::checked_cast<uint32_t>(input->size());
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
    raw_side_data(const scoped_refptr<media::DecoderBuffer>& input) {
  if (input->end_of_stream()) {
    return {};
  }
  // TODO(b/269383891): Remove this in M120.
  // If the receiver of the Mojo data is on an older version than us, then we
  // need to convert the side data to a raw format. We only care about spatial
  // layers since alpha data isn't used by HW decoders and the secure handle is
  // only going to used in new code going forward.
  if (!input->has_side_data() || input->side_data()->spatial_layers.empty()) {
    return {};
  }
  std::vector<uint8_t> raw_data;
  raw_data.resize(input->side_data()->spatial_layers.size() * sizeof(uint32_t));
  memcpy(raw_data.data(), input->side_data()->spatial_layers.data(),
         raw_data.size());
  return raw_data;
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
                   std::pair<base::TimeDelta, base::TimeDelta>>::value,
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
                   std::pair<base::TimeDelta, base::TimeDelta>>::value,
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
std::optional<media::DecoderBufferSideData>
StructTraits<media::stable::mojom::DecoderBufferDataView,
             scoped_refptr<media::DecoderBuffer>>::
    side_data(const scoped_refptr<media::DecoderBuffer>& input) {
  static_assert(
      std::is_same<decltype(input->side_data()),
                   std::optional<media::DecoderBufferSideData>>::value,
      "Unexpected type for input->side_data(). If you need to change this "
      "assertion, please contact chromeos-gfx-video@google.com.");
  if (input->end_of_stream()) {
    return std::nullopt;
  }
  return input->side_data();
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
  if (!input.ReadDuration(&duration)) {
    return false;
  }
  if (duration != media::kNoTimestamp &&
      (duration < base::TimeDelta() || duration == media::kInfiniteDuration)) {
    return false;
  }
  decoder_buffer->set_duration(duration);

  decoder_buffer->set_is_key_frame(input.is_key_frame());

  std::vector<uint8_t> raw_side_data;
  if (!input.ReadRawSideData(&raw_side_data)) {
    return false;
  }

  std::unique_ptr<media::DecryptConfig> decrypt_config;
  if (!input.ReadDecryptConfig(&decrypt_config))
    return false;
  if (decrypt_config)
    decoder_buffer->set_decrypt_config(std::move(decrypt_config));

  std::optional<media::DecoderBufferSideData> side_data;
  if (!input.ReadSideData(&side_data)) {
    return false;
  }
  decoder_buffer->set_side_data(side_data);

  // Note: DiscardPadding must be set after side data since the non-stable
  // interface has moved the discard padding into side data.
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
  if (discard_padding != media::DecoderBuffer::DiscardPadding()) {
    decoder_buffer->set_discard_padding(discard_padding);
  }

  // TODO(b/269383891): Remove this in M120.
  // If the input is an older version than us, then it may have |raw_side_data|
  // set and we need to copy that into the potential values in |side_data|.
  if (!raw_side_data.empty() && !side_data.has_value()) {
    // Spatial layers is always a multiple of 4 with a max size of 12.
    // HW decoders don't use alpha data, so we can ignore that case.
    if (raw_side_data.size() % sizeof(uint32_t) != 0 ||
        raw_side_data.size() > 3 * sizeof(uint32_t)) {
      return false;
    }
    decoder_buffer->WritableSideData().spatial_layers.resize(
        raw_side_data.size() / sizeof(uint32_t));
    memcpy(decoder_buffer->WritableSideData().spatial_layers.data(),
           raw_side_data.data(), raw_side_data.size());
  }

  *output = std::move(decoder_buffer);
  return true;
}

// static
uint64_t StructTraits<media::stable::mojom::DecoderBufferSideDataDataView,
                      media::DecoderBufferSideData>::
    secure_handle(media::DecoderBufferSideData input) {
  static_assert(
      std::is_same<decltype(input.secure_handle), uint64_t>::value,
      "Unexpected type for input.secure_handle. If you need to change this "
      "assertion, please contact chromeos-gfx-video@google.com.");
  return input.secure_handle;
}

// static
std::vector<uint32_t> StructTraits<
    media::stable::mojom::DecoderBufferSideDataDataView,
    media::DecoderBufferSideData>::spatial_layers(media::DecoderBufferSideData
                                                      input) {
  static_assert(
      std::is_same<decltype(input.spatial_layers),
                   std::vector<uint32_t>>::value,
      "Unexpected type for input.spatial_layers. If you need to change this "
      "assertion, please contact chromeos-gfx-video@google.com.");
  return input.spatial_layers;
}

// static
std::vector<uint8_t> StructTraits<
    media::stable::mojom::DecoderBufferSideDataDataView,
    media::DecoderBufferSideData>::alpha_data(media::DecoderBufferSideData
                                                  input) {
  static_assert(
      std::is_same<decltype(input.alpha_data), std::vector<uint8_t>>::value,
      "Unexpected type for input.alpha_data. If you need to change this "
      "assertion, please contact chromeos-gfx-video@google.com.");
  return input.alpha_data;
}

// static
bool StructTraits<media::stable::mojom::DecoderBufferSideDataDataView,
                  media::DecoderBufferSideData>::
    Read(media::stable::mojom::DecoderBufferSideDataDataView input,
         media::DecoderBufferSideData* output) {
  constexpr size_t kMaxSpatialLayers = 3;
  if (!input.ReadSpatialLayers(&output->spatial_layers) ||
      output->spatial_layers.size() > kMaxSpatialLayers) {
    return false;
  }
  if (!input.ReadAlphaData(&output->alpha_data)) {
    return false;
  }
  output->secure_handle = input.secure_handle();
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
const std::optional<media::EncryptionPattern>&
StructTraits<media::stable::mojom::DecryptConfigDataView,
             std::unique_ptr<media::DecryptConfig>>::
    encryption_pattern(const std::unique_ptr<media::DecryptConfig>& input) {
  static_assert(
      std::is_same<decltype(input->encryption_pattern()),
                   std::add_lvalue_reference<std::add_const<
                       decltype(media::stable::mojom::DecryptConfig::
                                    encryption_pattern)>::type>::type>::value,
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
  if (encryption_scheme == media::EncryptionScheme::kUnencrypted) {
    // The DecryptConfig constructor has a DCHECK() that rejects
    // EncryptionScheme::kUnencrypted.
    return false;
  }

  std::string key_id;
  if (!input.ReadKeyId(&key_id))
    return false;
  if (key_id.empty()) {
    return false;
  }

  std::string iv;
  if (!input.ReadIv(&iv))
    return false;
  if (iv.size() !=
      static_cast<size_t>(media::DecryptConfig::kDecryptionKeySize)) {
    return false;
  }

  std::vector<media::SubsampleEntry> subsamples;
  if (!input.ReadSubsamples(&subsamples))
    return false;

  std::optional<media::EncryptionPattern> encryption_pattern;
  if (!input.ReadEncryptionPattern(&encryption_pattern))
    return false;
  if (encryption_scheme != media::EncryptionScheme::kCbcs &&
      encryption_pattern.has_value()) {
    return false;
  }

  *output = std::make_unique<media::DecryptConfig>(
      encryption_scheme, key_id, iv, subsamples, encryption_pattern);
  return true;
}

// static
uint32_t StructTraits<
    media::stable::mojom::HDRMetadataDataView,
    gfx::HDRMetadata>::max_content_light_level(const gfx::HDRMetadata& input) {
  auto cta_861_3 = input.cta_861_3.value_or(gfx::HdrMetadataCta861_3());
  static_assert(
      std::is_same<decltype(cta_861_3.max_content_light_level),
                   decltype(media::stable::mojom::HDRMetadata::
                                max_content_light_level)>::value,
      "Unexpected type for gfx::HdrMetadataCta861_3::max_content_light_level. "
      "If you need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return cta_861_3.max_content_light_level;
}

// static
uint32_t
StructTraits<media::stable::mojom::HDRMetadataDataView, gfx::HDRMetadata>::
    max_frame_average_light_level(const gfx::HDRMetadata& input) {
  auto cta_861_3 = input.cta_861_3.value_or(gfx::HdrMetadataCta861_3());
  static_assert(
      std::is_same<decltype(cta_861_3.max_frame_average_light_level),
                   decltype(media::stable::mojom::HDRMetadata::
                                max_frame_average_light_level)>::value,
      "Unexpected type for "
      "gfx::HdrMetadataCta861_3::max_frame_average_light_level. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return cta_861_3.max_frame_average_light_level;
}

// static
gfx::HdrMetadataSmpteSt2086 StructTraits<
    media::stable::mojom::HDRMetadataDataView,
    gfx::HDRMetadata>::color_volume_metadata(const gfx::HDRMetadata& input) {
  auto smpte_st_2086 =
      input.smpte_st_2086.value_or(gfx::HdrMetadataSmpteSt2086());
  static_assert(
      std::is_same<decltype(smpte_st_2086),
                   decltype(media::stable::mojom::HDRMetadata::
                                color_volume_metadata)>::value,
      "Unexpected type for gfx::HDRMetadata::smpte_st_2086. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");

  return smpte_st_2086;
}

// static
bool StructTraits<media::stable::mojom::HDRMetadataDataView, gfx::HDRMetadata>::
    Read(media::stable::mojom::HDRMetadataDataView data,
         gfx::HDRMetadata* output) {
  gfx::HdrMetadataCta861_3 cta_861_3(data.max_content_light_level(),
                                     data.max_frame_average_light_level());
  gfx::HdrMetadataSmpteSt2086 smpte_st_2086;
  if (!data.ReadColorVolumeMetadata(&smpte_st_2086)) {
    return false;
  }
  output->cta_861_3 = cta_861_3;
  output->smpte_st_2086 = smpte_st_2086;
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

  // When creating an "ok" status, callers are expected to not supply anything
  // (except possibly the kOk code) which should result in a media::TypedStatus
  // with no StatusData. That means that we should never get a StatusData with a
  // kOk code when serializing a DecoderStatus.
  CHECK_NE(input.code,
           static_cast<uint16_t>(media::DecoderStatusTraits::Codes::kOk));

  if (input.code ==
      static_cast<uint16_t>(media::DecoderStatusTraits::Codes::kAborted)) {
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
const base::Value::List& StructTraits<media::stable::mojom::StatusDataDataView,
                                      media::internal::StatusData>::
    frames(const media::internal::StatusData& input) {
  // Note that types of `internal::StatusData::frames` and
  // `mojom::StatusData::frames` do not match -- changing the wire format of
  // stable mojom requires supporting both the old and new versions and doesn't
  // improve readability.
  static_assert(
      std::is_same_v<decltype(::media::internal::StatusData::frames),
                     base::Value::List>,
      "Unexpected type for media::internal::StatusData::frames. If you need to "
      "change this assertion, please contact chromeos-gfx-video@google.com.");
  return input.frames;
}

// static
std::optional<media::internal::StatusData> StructTraits<
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

    // When creating an "ok" status, callers are expected to not supply anything
    // (except possibly the kOk code) which should result in a
    // media::TypedStatus<T> with no StatusData. That means that we should never
    // get a cause with a kOk code when serializing a VaapiStatus or V4L2Status.
    //
    // TODO(b/215438024): enforce that the checks on Group() imply that the
    // output_cause.code really corresponds to media::VaapiStatusTraits::Codes
    // or media::V4L2StatusTraits::Codes.
#if BUILDFLAG(USE_VAAPI)
    CHECK(output_cause.group == media::VaapiStatusTraits::Group());
    CHECK_NE(output_cause.code, static_cast<media::StatusCodeType>(
                                    media::VaapiStatusTraits::Codes::kOk));
#elif BUILDFLAG(USE_V4L2_CODEC)
    CHECK(output_cause.group == media::V4L2StatusTraits::Group());
    CHECK_NE(output_cause.code, static_cast<media::StatusCodeType>(
                                    media::V4L2StatusTraits::Codes::kOk));
#else
    // TODO(b/217970098): allow building the VaapiStatusTraits and
    // V4L2StatusTraits without USE_VAAPI/USE_V4L2_CODEC so these guards could
    // be removed.
    CHECK(false);
#endif
    // Let's translate anything other than a VA-API or V4L2 "ok" cause (i.e.,
    // all of them per the CHECK()s above) to
    // DecoderStatusTraits::Codes::kFailed.
    output_cause.code = static_cast<media::StatusCodeType>(
        media::DecoderStatusTraits::Codes::kFailed);
    output_cause.group = std::string(media::DecoderStatusTraits::Group());
    return output_cause;
  }
  return std::nullopt;
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

  // Note that we don't handle kOk_DEPRECATED here. That's because when creating
  // an "ok" status, callers are expected to not supply anything (except
  // possibly the kOk code) which should result in a media::TypedStatus<T> with
  // no StatusData. That means that we should never get a StatusData with an
  // "ok" code from the remote end. kOk_DEPRECATED was fine back when
  // TypedStatus<T>::is_ok() relied on the status code and not on the
  // presence/absence of a StatusData.
  switch (data.code()) {
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

  // Ensure that |output|->frames looks like a list of serialized
  // base::Locations. See media::MediaSerializer<base::Location>.
  for (const auto& frame : output->frames) {
    if (!frame.is_dict()) {
      return false;
    }
    const base::Value::Dict& dict = frame.GetDict();
    if (dict.size() != 2u) {
      return false;
    }
    const std::string* file = dict.FindString(media::StatusConstants::kFileKey);
    if (!file) {
      return false;
    }
    const std::optional<int> line =
        dict.FindInt(media::StatusConstants::kLineKey);
    if (!line) {
      return false;
    }
  }

  if (!data.ReadData(&output->data))
    return false;

  std::optional<media::internal::StatusData> cause;
  if (!data.ReadCause(&cause))
    return false;

  if (cause.has_value()) {
    // The deserialization of a cause (a StatusData) translates
    // media::stable::mojom::StatusCodes to media::DecoderStatusTraits::Codes.
    CHECK(cause->group == media::DecoderStatusTraits::Group());
    if (cause->code != static_cast<media::StatusCodeType>(
                           media::DecoderStatusTraits::Codes::kFailed)) {
      return false;
    }
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

  return mojo::OptionalAsPointer(input.data_.get());
}

// static
bool StructTraits<media::stable::mojom::StatusDataView, media::DecoderStatus>::
    Read(media::stable::mojom::StatusDataView data,
         media::DecoderStatus* output) {
  std::optional<media::internal::StatusData> internal;
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
  if (!input.ReadProfileMin(&output->profile_min) ||
      output->profile_min == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }

  if (!input.ReadProfileMax(&output->profile_max) ||
      output->profile_max == media::VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }

  if (output->profile_max < output->profile_min) {
    return false;
  }

  if (!input.ReadCodedSizeMin(&output->coded_size_min))
    return false;

  if (!input.ReadCodedSizeMax(&output->coded_size_max))
    return false;

  if (output->coded_size_max.width() <= output->coded_size_min.width() ||
      output->coded_size_max.height() <= output->coded_size_min.height()) {
    return false;
  }

  if (input.require_encrypted() && !input.allow_encrypted()) {
    // Inconsistent information.
    return false;
  }

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
const std::optional<gfx::HDRMetadata>& StructTraits<
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

  std::optional<gfx::HDRMetadata> hdr_metadata;
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
    needs_detiling(const media::VideoFrameMetadata& input) {
  static_assert(
      std::is_same<decltype(::media::VideoFrameMetadata::needs_detiling),
                   decltype(media::stable::mojom::VideoFrameMetadata::
                                needs_detiling)>::value,
      "Unexpected type for media::VideoFrameMetadata::needs_detiling. If you "
      "need to change this assertion, please contact "
      "chromeos-gfx-video@google.com.");

  return input.needs_detiling;
}

// static
bool StructTraits<media::stable::mojom::VideoFrameMetadataDataView,
                  media::VideoFrameMetadata>::
    Read(media::stable::mojom::VideoFrameMetadataDataView input,
         media::VideoFrameMetadata* output) {
  output->allow_overlay = true;
  output->end_of_stream = false;
  output->read_lock_fences_enabled = true;
  output->protected_video = input.protected_video();
  output->hw_protected = input.hw_protected();
  output->needs_detiling = input.needs_detiling();
  output->power_efficient = true;

  if (output->hw_protected && !output->protected_video) {
    // According to the VideoFrameMetadata documentation, |hw_protected| is only
    // valid if |protected_video| is set to true.
    return false;
  }

  return true;
}

}  // namespace mojo
