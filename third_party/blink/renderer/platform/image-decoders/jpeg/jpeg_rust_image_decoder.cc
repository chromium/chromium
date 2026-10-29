// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_rust_image_decoder.h"

#include <array>

#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/skia/experimental/rust_jpeg/decoder/SkJpegRustDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/private/SkJpegMetadataDecoder.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

JpegRustImageDecoder::~JpegRustImageDecoder() = default;

String JpegRustImageDecoder::FilenameExtension() const {
  return "jpg";
}

const AtomicString& JpegRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, jpeg_mime_type, ("image/jpeg"));
  return jpeg_mime_type;
}

void JpegRustImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  SkiaImageDecoderBase::OnSetData(data);
  allow_decode_to_yuv_ = false;
  if (!data || !codec() || !IsAllDataReceived() ||
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
  std::unique_ptr<SkCodec> codec =
      SkJpegRustDecoder::Decode(std::move(stream), result);
  return codec;
}

void JpegRustImageDecoder::OnSkCodecCreated() {
  DCHECK(data_);
  sk_sp<const SkData> encoded_data = data_->GetAsSkData();
  DCHECK(encoded_data);

  auto metadata_decoder = SkJpegMetadataDecoder::Make(encoded_data);
  DCHECK(metadata_decoder);
  // The base already applied SkCodec's EXIF orientation. Reuse the legacy JPEG
  // helper to also compute Blink's density-corrected size; assigning the same
  // orientation again is harmless.
  ApplyExifMetadata(metadata_decoder->getExifMetadata(/*copyData=*/false).get(),
                    Size());
}

}  // namespace blink
