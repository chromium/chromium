// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_rust_image_decoder.h"

#include <algorithm>
#include <array>

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_jpeg/decoder/SkJpegRustDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/private/SkJpegMetadataDecoder.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

JpegRustImageDecoder::JpegRustImageDecoder(AlphaOption alpha_option,
                                           ColorBehavior color_behavior,
                                           cc::AuxImage aux_image,
                                           wtf_size_t max_decoded_bytes,
                                           const gfx::Size& desired_size)
    : SkiaImageDecoderBase(alpha_option,
                           color_behavior,
                           max_decoded_bytes,
                           kNoReadingOffset,
                           kDefaultBitDepth,
                           desired_size,
                           aux_image) {}

JpegRustImageDecoder::~JpegRustImageDecoder() = default;

gfx::Size JpegRustImageDecoder::DecodedSize() const {
  return decoded_size_;
}

Vector<SkISize> JpegRustImageDecoder::GetSupportedDecodeSizes() const {
  DCHECK(IsDecodedSizeAvailable());
  return supported_decode_sizes_;
}

String JpegRustImageDecoder::FilenameExtension() const {
  return "jpg";
}

const AtomicString& JpegRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, jpeg_mime_type,
                      (AtomicString::FromLiteral<"image/jpeg">()));
  return jpeg_mime_type;
}

void JpegRustImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  allow_decode_to_yuv_ = false;
  if (data && aux_image_ == cc::AuxImage::kGainmap) {
    if (!gainmap_data_) {
      // The gainmap may follow the main JPEG. Do not initialize a codec with
      // the main image while the container is still arriving.
      if (!IsAllDataReceived()) {
        data_.reset();
        SkiaImageDecoderBase::OnSetData(nullptr);
        return;
      }
      auto encoded_data = data->GetAsSkData();
      if (!encoded_data) {
        SetFailed();
        return;
      }
      auto metadata_decoder = SkJpegMetadataDecoder::Make(encoded_data);
      if (!metadata_decoder) {
        SetFailed();
        return;
      }
      auto [gainmap_data, gainmap_info] =
          metadata_decoder->findGainmapImage(encoded_data);
      if (!gainmap_data) {
        SetFailed();
        return;
      }
      gainmap_data_ = SegmentReader::CreateFromSkData(std::move(gainmap_data));
    }
    data = gainmap_data_;
    // RGB, YUV and metadata decoding must all use the selected JPEG.
    data_ = data;
  }
  SkiaImageDecoderBase::OnSetData(data);
  if (!data || !codec() || DecodedSize() != Size() || !IsAllDataReceived() ||
      NeedsDecodeTimeColorTransform()) {
    return;
  }

  SkYUVAPixmapInfo::SupportedDataTypes supported_data_types;
  supported_data_types.enableDataType(SkYUVAPixmapInfo::DataType::kUnorm8, 1);
  // Blink applies EXIF orientation separately from YUV plane decoding. Use RGB
  // decoding when Skia's orientation-adjusted plane dimensions do not match the
  // coded size.
  if (!codec()->queryYUVAInfo(supported_data_types, &yuva_pixmap_info_) ||
      yuva_pixmap_info_.yuvaInfo().planeConfig() !=
          SkYUVAInfo::PlaneConfig::kY_U_V ||
      gfx::SkISizeToSize(yuva_pixmap_info_.planeInfo(0).dimensions()) !=
          Size()) {
    return;
  }

  switch (yuva_pixmap_info_.yuvaInfo().subsampling()) {
    case SkYUVAInfo::Subsampling::k420:
    case SkYUVAInfo::Subsampling::k422:
    case SkYUVAInfo::Subsampling::k444:
      allow_decode_to_yuv_ = true;
      break;
    default:
      break;
  }
}

cc::YUVSubsampling JpegRustImageDecoder::GetYUVSubsampling() const {
  switch (yuva_pixmap_info_.yuvaInfo().subsampling()) {
    case SkYUVAInfo::Subsampling::k420:
      return cc::YUVSubsampling::k420;
    case SkYUVAInfo::Subsampling::k422:
      return cc::YUVSubsampling::k422;
    case SkYUVAInfo::Subsampling::k444:
      return cc::YUVSubsampling::k444;
    default:
      return cc::YUVSubsampling::kUnknown;
  }
}

gfx::Size JpegRustImageDecoder::DecodedYUVSize(cc::YUVIndex index) const {
  return gfx::SkISizeToSize(
      yuva_pixmap_info_.planeInfo(static_cast<int>(index)).dimensions());
}

wtf_size_t JpegRustImageDecoder::DecodedYUVWidthBytes(
    cc::YUVIndex index) const {
  return yuva_pixmap_info_.rowBytes(static_cast<int>(index));
}

void JpegRustImageDecoder::DecodeToYUV() {
  DCHECK(CanDecodeToYUV());
  DCHECK(image_planes_);
  DCHECK_EQ(image_planes_->color_type(), kGray_8_SkColorType);

  allow_decode_to_yuv_ = false;
  if (Failed()) {
    return;
  }

  std::array<SkPixmap, SkYUVAPixmaps::kMaxPlanes> pixmaps;
  for (int plane = 0; plane < cc::kNumYUVPlanes; ++plane) {
    const auto index = static_cast<cc::YUVIndex>(plane);
    pixmaps[plane] =
        SkPixmap(yuva_pixmap_info_.planeInfo(plane),
                 image_planes_->Plane(index), image_planes_->RowBytes(index));
  }
  const SkYUVAPixmaps yuva_pixmaps = SkYUVAPixmaps::FromExternalPixmaps(
      yuva_pixmap_info_.yuvaInfo(), pixmaps.data());
  if (!yuva_pixmaps.isValid()) {
    SetFailed();
    return;
  }

  // Raw YUV decoding is non-incremental here. An immutable stream distinguishes
  // final EOF from a temporary suspension and preserves truncated JPEG output.
  SkCodec::Result result;
  auto complete_codec =
      SkJpegRustDecoder::Decode(data_->GetAsSkData(), &result);
  if (!complete_codec) {
    SetFailed();
    return;
  }
  result = complete_codec->getYUVAPlanes(yuva_pixmaps);
  if (result != SkCodec::kSuccess) {
    SetFailed();
    return;
  }
  image_planes_->SetHasCompleteScan();
  allow_decode_to_yuv_ = true;
}

SkYUVColorSpace JpegRustImageDecoder::GetYUVColorSpace() const {
  return yuva_pixmap_info_.yuvColorSpace();
}

bool JpegRustImageDecoder::GetGainmapInfoAndData(
    SkGainmapInfo& out_gainmap_info,
    scoped_refptr<SegmentReader>& out_gainmap_data) const {
  if (!data_) {
    return false;
  }

  sk_sp<const SkData> encoded_data = data_->GetAsSkData();
  if (!encoded_data) {
    return false;
  }
  auto metadata_decoder = SkJpegMetadataDecoder::Make(encoded_data);
  if (!metadata_decoder) {
    return false;
  }

  auto [gainmap_data, gainmap_info] =
      metadata_decoder->findGainmapImage(encoded_data);
  if (!gainmap_data) {
    return false;
  }
  out_gainmap_info = gainmap_info;

  out_gainmap_data = data_;
  return true;
}

std::unique_ptr<SkCodec> JpegRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  SkJpegRustDecoder::Metadata metadata;
  codec_input_complete_ = IsAllDataReceived();
  const auto input_state = codec_input_complete_
                               ? SkJpegRustDecoder::InputState::kComplete
                               : SkJpegRustDecoder::InputState::kGrowing;
  std::unique_ptr<SkCodec> codec = SkJpegRustDecoder::Decode(
      std::move(stream), result, input_state, &metadata);
  if (codec) {
    source_mcu_size_ = gfx::SkISizeToSize(metadata.fMCUSize);
    if (source_mcu_size_.IsEmpty()) {
      if (result) {
        *result = SkCodec::kErrorInInput;
      }
      return nullptr;
    }
  }
  return codec;
}

SkCodec::Result JpegRustImageDecoder::OnIncrementalDecode(wtf_size_t index) {
  SkCodec::Result result = SkiaImageDecoderBase::OnIncrementalDecode(index);
  // Retry only when a codec created for growing input still reports
  // kIncompleteInput after all data has arrived (e.g. missing EOI). Growing
  // codecs treat EOF as temporary; a complete-input codec finalizes it.
  if (result != SkCodec::kIncompleteInput || !IsAllDataReceived() ||
      codec_input_complete_ || retried_complete_input_) {
    return result;
  }
  retried_complete_input_ = true;
  DCHECK_EQ(index, 0u);
  DCHECK(data_);
  auto encoded_data = data_->GetAsSkData();
  if (!encoded_data) {
    return SkCodec::kErrorInInput;
  }

  auto complete_codec = SkJpegRustDecoder::Decode(encoded_data, &result);
  if (!complete_codec) {
    return result;
  }
  ImageFrame& frame = frame_buffer_cache_[index];
  frame.ZeroFillPixelData();
  const SkImageInfo image_info =
      frame.Bitmap()
          .info()
          .makeAlphaType(kOpaque_SkAlphaType)
          .makeColorSpace(NeedsDecodeTimeColorTransform()
                              ? ColorSpaceForSkImages()
                              : nullptr);
  result = complete_codec->startIncrementalDecode(
      image_info, frame.Bitmap().getPixels(), frame.Bitmap().rowBytes());
  if (result != SkCodec::kSuccess) {
    return result;
  }
  return complete_codec->incrementalDecode();
}

void JpegRustImageDecoder::OnSkCodecCreated() {
  constexpr unsigned kScaleDenominator = 8;
  wtf_size_t target_bytes = max_decoded_bytes_;
  if (!desired_size_.IsEmpty()) {
    target_bytes = std::min(
        target_bytes, static_cast<wtf_size_t>(desired_size_.Area64() * 4));
  }
  const unsigned max_numerator = JPEGImageDecoder::DesiredScaleNumerator(
      target_bytes, static_cast<wtf_size_t>(Size().Area64() * 4),
      kScaleDenominator);

  supported_decode_sizes_.clear();
  // Preserve native partial-MCU protection unless the budget forces
  // downsampling.
  if (max_numerator == kScaleDenominator &&
      (Size().width() % source_mcu_size_.width() != 0 ||
       Size().height() % source_mcu_size_.height() != 0)) {
    supported_decode_sizes_.push_back(gfx::SizeToSkISize(Size()));
  } else {
    for (unsigned numerator = 1; numerator <= max_numerator; ++numerator) {
      const SkISize size = codec()->getScaledDimensions(
          static_cast<float>(numerator) / kScaleDenominator);
      if (supported_decode_sizes_.empty() ||
          supported_decode_sizes_.back() != size) {
        supported_decode_sizes_.push_back(size);
      }
    }
  }
  decoded_size_ = gfx::SkISizeToSize(supported_decode_sizes_.back());
  DCHECK(data_);
  sk_sp<const SkData> encoded_data = data_->GetAsSkData();
  DCHECK(encoded_data);

  auto metadata_decoder = SkJpegMetadataDecoder::Make(encoded_data);
  DCHECK(metadata_decoder);
  // The base already applied SkCodec's EXIF orientation. Reuse the legacy JPEG
  // helper to also compute Blink's density-corrected size; assigning the same
  // orientation again is harmless.
  ApplyExifMetadata(metadata_decoder->getExifMetadata(/*copyData=*/false).get(),
                    decoded_size_);
}

}  // namespace blink
