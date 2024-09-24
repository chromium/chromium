// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_core.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace blink {

namespace {

media::VideoPixelFormat YUVSubsamplingToMediaPixelFormat(
    cc::YUVSubsampling sampling,
    int depth) {
  // TODO(crbug.com/1073995): Add support for high bit depth format.
  if (depth != 8)
    return media::PIXEL_FORMAT_UNKNOWN;

  switch (sampling) {
    case cc::YUVSubsampling::k420:
      return media::PIXEL_FORMAT_I420;
    case cc::YUVSubsampling::k422:
      return media::PIXEL_FORMAT_I422;
    case cc::YUVSubsampling::k444:
      return media::PIXEL_FORMAT_I444;
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

std::pair<gfx::ColorSpace::PrimaryID, gfx::ColorSpace::TransferID>
GuessPrimaryAndTransfer(SkYUVColorSpace yuv_cs) {
  switch (yuv_cs) {
    case kJPEG_Full_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::BT709,
              gfx::ColorSpace::TransferID::SRGB};
    case kFCC_Full_SkYUVColorSpace:
    case kFCC_Limited_SkYUVColorSpace:
    case kRec601_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::SMPTE170M,
              gfx::ColorSpace::TransferID::SMPTE170M};
    case kRec709_Limited_SkYUVColorSpace:
    case kRec709_Full_SkYUVColorSpace:
    // Unclear what these should be, so guess BT.709.
    case kYDZDX_Full_SkYUVColorSpace:
    case kYDZDX_Limited_SkYUVColorSpace:
    case kYCgCo_8bit_Full_SkYUVColorSpace:
    case kYCgCo_10bit_Full_SkYUVColorSpace:
    case kYCgCo_12bit_Full_SkYUVColorSpace:
    case kYCgCo_16bit_Full_SkYUVColorSpace:
    case kYCgCo_8bit_Limited_SkYUVColorSpace:
    case kYCgCo_10bit_Limited_SkYUVColorSpace:
    case kYCgCo_12bit_Limited_SkYUVColorSpace:
    case kYCgCo_16bit_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::BT709,
              gfx::ColorSpace::TransferID::BT709};
    case kBT2020_8bit_Full_SkYUVColorSpace:
    case kBT2020_10bit_Full_SkYUVColorSpace:
    case kBT2020_8bit_Limited_SkYUVColorSpace:
    case kBT2020_10bit_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::BT2020,
              gfx::ColorSpace::TransferID::BT2020_10};
    case kBT2020_12bit_Full_SkYUVColorSpace:
    case kBT2020_16bit_Full_SkYUVColorSpace:
    case kBT2020_12bit_Limited_SkYUVColorSpace:
    case kBT2020_16bit_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::BT2020,
              gfx::ColorSpace::TransferID::BT2020_12};
    case kSMPTE240_Full_SkYUVColorSpace:
    case kSMPTE240_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::SMPTE240M,
              gfx::ColorSpace::TransferID::SMPTE240M};
    case kGBR_Full_SkYUVColorSpace:
    case kGBR_Limited_SkYUVColorSpace:
      return {gfx::ColorSpace::PrimaryID::BT709,
              gfx::ColorSpace::TransferID::SRGB};
    case kIdentity_SkYUVColorSpace:
      NOTREACHED();
  };
}

gfx::ColorSpace YUVColorSpaceToGfxColorSpace(SkYUVColorSpace yuv_cs,
                                             const gfx::ColorSpace& gfx_cs) {
  auto primary_id = gfx_cs.GetPrimaryID();
  auto transfer_id = gfx_cs.GetTransferID();
  if (!gfx_cs.IsValid()) {
    std::tie(primary_id, transfer_id) = GuessPrimaryAndTransfer(yuv_cs);
  }

  switch (yuv_cs) {
    case kJPEG_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::SMPTE170M,
                             gfx::ColorSpace::RangeID::FULL);
    case kRec601_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::SMPTE170M,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kRec709_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::BT709,
                             gfx::ColorSpace::RangeID::FULL);
    case kRec709_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::BT709,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kBT2020_8bit_Full_SkYUVColorSpace:
    case kBT2020_10bit_Full_SkYUVColorSpace:
    case kBT2020_12bit_Full_SkYUVColorSpace:
    case kBT2020_16bit_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::BT2020_NCL,
                             gfx::ColorSpace::RangeID::FULL);
    case kBT2020_8bit_Limited_SkYUVColorSpace:
    case kBT2020_10bit_Limited_SkYUVColorSpace:
    case kBT2020_12bit_Limited_SkYUVColorSpace:
    case kBT2020_16bit_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::BT2020_NCL,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kFCC_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::FCC,
                             gfx::ColorSpace::RangeID::FULL);
    case kFCC_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::FCC,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kSMPTE240_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::SMPTE240M,
                             gfx::ColorSpace::RangeID::FULL);
    case kSMPTE240_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::SMPTE240M,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kYDZDX_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::YDZDX,
                             gfx::ColorSpace::RangeID::FULL);
    case kYDZDX_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::YDZDX,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kGBR_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::GBR,
                             gfx::ColorSpace::RangeID::FULL);
    case kGBR_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::GBR,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kYCgCo_8bit_Full_SkYUVColorSpace:
    case kYCgCo_10bit_Full_SkYUVColorSpace:
    case kYCgCo_12bit_Full_SkYUVColorSpace:
    case kYCgCo_16bit_Full_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::YCOCG,
                             gfx::ColorSpace::RangeID::FULL);
    case kYCgCo_8bit_Limited_SkYUVColorSpace:
    case kYCgCo_10bit_Limited_SkYUVColorSpace:
    case kYCgCo_12bit_Limited_SkYUVColorSpace:
    case kYCgCo_16bit_Limited_SkYUVColorSpace:
      return gfx::ColorSpace(primary_id, transfer_id,
                             gfx::ColorSpace::MatrixID::YCOCG,
                             gfx::ColorSpace::RangeID::LIMITED);
    case kIdentity_SkYUVColorSpace:
      NOTREACHED_IN_MIGRATION();
      return gfx::ColorSpace();
  };
}

}  // namespace

ImageDecoderCore::ImageDecoderCore(
    String mime_type,
    scoped_refptr<SegmentReader> data,
    bool data_complete,
    ColorBehavior color_behavior,
    const SkISize& desired_size,
    ImageDecoder::AnimationOption animation_option)
    : mime_type_(mime_type),
      color_behavior_(color_behavior),
      desired_size_(desired_size),
      animation_option_(animation_option),
      data_complete_(data_complete),
      segment_reader_(std::move(data)) {
  if (!segment_reader_) {
    stream_buffer_ = WTF::SharedBuffer::Create();
    segment_reader_ = SegmentReader::CreateFromSharedBuffer(stream_buffer_);
  }

  Reinitialize(animation_option_);

  base::UmaHistogramEnumeration("Blink.WebCodecs.ImageDecoder.Type",
                                BitmapImageMetrics::StringToDecodedImageType(
                                    decoder_->FilenameExtension()));
}

ImageDecoderCore::~ImageDecoderCore() = default;

ImageDecoderCore::ImageMetadata ImageDecoderCore::DecodeMetadata() {
  DCHECK(decoder_);

  ImageDecoderCore::ImageMetadata metadata;
  metadata.data_complete = data_complete_;

  if (!decoder_->IsSizeAvailable()) {
    // Decoding has failed if we have no size and no more data.
    metadata.failed = decoder_->Failed() || data_complete_;
    return metadata;
  }

  metadata.has_size = true;
  metadata.frame_count = base::checked_cast<uint32_t>(decoder_->FrameCount());
  metadata.repetition_count = decoder_->RepetitionCount();
  metadata.image_has_both_still_and_animated_sub_images =
      decoder_->ImageHasBothStillAndAnimatedSubImages();

  // It's important that |failed| is set last since some of the methods above
  // may trigger operations which can lead to failure.
  metadata.failed = decoder_->Failed();
  return metadata;
}

std::unique_ptr<ImageDecoderCore::ImageDecodeResult> ImageDecoderCore::Decode(
    uint32_t frame_index,
    bool complete_frames_only,
    const base::AtomicFlag* abort_flag) {
  DCHECK(decoder_);

  auto result = std::make_unique<ImageDecodeResult>();
  result->frame_index = frame_index;

  if (abort_flag->IsSet()) {
    result->status = Status::kAborted;
    return result;
  }

  if (decoder_->Failed()) {
    result->status = Status::kDecodeError;
    return result;
  }

  if (!decoder_->IsSizeAvailable()) {
    result->status = Status::kNoImage;
    return result;
  }

  if (data_complete_ && frame_index >= decoder_->FrameCount()) {
    result->status = Status::kIndexError;
    return result;
  }

  // Due to implementation limitations YUV support for some formats is only
  // known once all data is received. Animated images are never supported.
  if (decoder_->CanDecodeToYUV() && !have_completed_rgb_decode_ &&
      frame_index == 0u) {
    if (!have_completed_yuv_decode_) {
      MaybeDecodeToYuv();
      if (decoder_->Failed()) {
        result->status = Status::kDecodeError;
        return result;
      }
    }

    if (have_completed_yuv_decode_) {
      result->status = Status::kOk;
      result->frame = yuv_frame_;
      result->complete = true;
      return result;
    }
  }

  auto* image = decoder_->DecodeFrameBufferAtIndex(frame_index);
  if (decoder_->Failed()) {
    result->status = Status::kDecodeError;
    return result;
  }

  if (!image) {
    result->status = Status::kNoImage;
    return result;
  }

  // Nothing to do if nothing has been decoded yet.
  if (image->GetStatus() == ImageFrame::kFrameEmpty ||
      image->GetStatus() == ImageFrame::kFrameInitialized) {
    result->status = Status::kNoImage;
    return result;
  }

  have_completed_rgb_decode_ = true;

  // Only satisfy fully complete decode requests. Treat partial decodes as
  // complete if we've received all the data we ever will.
  const bool is_complete = image->GetStatus() == ImageFrame::kFrameComplete;
  if (!is_complete && complete_frames_only) {
    result->status = Status::kNoImage;
    return result;
  }

  // Prefer FinalizePixelsAndGetImage() since that will mark the underlying
  // bitmap as immutable, which allows copies to be avoided.
  auto sk_image = is_complete ? image->FinalizePixelsAndGetImage()
                              : SkImages::RasterFromBitmap(image->Bitmap());
  if (!sk_image) {
    NOTREACHED_IN_MIGRATION()
        << "Failed to retrieve SkImage for decoded image.";
    result->status = Status::kDecodeError;
    return result;
  }

  if (!is_complete) {
    auto generation_id = image->Bitmap().getGenerationID();
    auto it = incomplete_frames_.find(frame_index);
    if (it == incomplete_frames_.end()) {
      incomplete_frames_.Set(frame_index, generation_id);
    } else {
      // Don't fulfill the promise until a new bitmap is seen.
      if (it->value == generation_id) {
        result->status = Status::kNoImage;
        return result;
      }

      it->value = generation_id;
    }
  } else {
    incomplete_frames_.erase(frame_index);
  }

  // This is zero copy; the VideoFrame points into the SkBitmap.
  const gfx::Size coded_size(sk_image->width(), sk_image->height());
  auto frame =
      media::CreateFromSkImage(sk_image, gfx::Rect(coded_size), coded_size,
                               GetTimestampForFrame(frame_index));
  if (!frame) {
    NOTREACHED_IN_MIGRATION() << "Failed to create VideoFrame from SkImage.";
    result->status = Status::kDecodeError;
    return result;
  }

  if (auto sk_cs = decoder_->ColorSpaceForSkImages()) {
    auto gfx_cs = gfx::ColorSpace(*sk_cs);
    if (gfx_cs.IsValid()) {
      frame->set_color_space(gfx_cs);
    }
  }

  frame->metadata().transformation =
      ImageOrientationToVideoTransformation(decoder_->Orientation());

  // Only animated images have frame durations.
  if (decoder_->FrameCount() > 1 ||
      decoder_->RepetitionCount() != kAnimationNone) {
    frame->metadata().frame_duration =
        decoder_->FrameDurationAtIndex(frame_index);
  }

  if (is_decoding_in_order_) {
    // Stop aggressive purging when out of order decoding is detected.
    if (last_decoded_frame_ != frame_index &&
        ((last_decoded_frame_ + 1) % decoder_->FrameCount()) != frame_index) {
      is_decoding_in_order_ = false;
    } else {
      decoder_->ClearCacheExceptFrame(frame_index);
    }
    last_decoded_frame_ = frame_index;
  }

  result->status = Status::kOk;
  result->sk_image = std::move(sk_image);
  result->frame = std::move(frame);
  result->complete = is_complete;
  return result;
}

void ImageDecoderCore::AppendData(size_t data_size,
                                  std::unique_ptr<uint8_t[]> data,
                                  bool data_complete) {
  DCHECK(stream_buffer_);
  DCHECK(stream_buffer_);
  DCHECK(!data_complete_);
  data_complete_ = data_complete;
  if (data) {
    stream_buffer_->Append(reinterpret_cast<const char*>(data.get()),
                           base::checked_cast<wtf_size_t>(data_size));
  } else {
    DCHECK_EQ(data_size, 0u);
  }

  // We may not have a decoder if Clear() was called while data arrives.
  if (decoder_)
    decoder_->SetData(stream_buffer_, data_complete_);
}

void ImageDecoderCore::Clear() {
  decoder_.reset();
  incomplete_frames_.clear();
  yuv_frame_ = nullptr;
  have_completed_rgb_decode_ = false;
  have_completed_yuv_decode_ = false;
  last_decoded_frame_ = 0u;
  is_decoding_in_order_ = true;
  timestamp_cache_.clear();
  timestamp_cache_.emplace_back();
}

void ImageDecoderCore::Reinitialize(
    ImageDecoder::AnimationOption animation_option) {
  Clear();
  animation_option_ = animation_option;
  decoder_ = ImageDecoder::CreateByMimeType(
      mime_type_, segment_reader_, data_complete_,
      ImageDecoder::kAlphaNotPremultiplied,
      ImageDecoder::HighBitDepthDecodingOption::kDefaultBitDepth,
      color_behavior_, cc::AuxImage::kDefault,
      Platform::GetMaxDecodedImageBytes(), desired_size_, animation_option_);
  DCHECK(decoder_);
}

bool ImageDecoderCore::FrameIsDecodedAtIndexForTesting(
    uint32_t frame_index) const {
  return decoder_->FrameIsDecodedAtIndex(frame_index);
}

void ImageDecoderCore::MaybeDecodeToYuv() {
  DCHECK(!have_completed_rgb_decode_);
  DCHECK(!have_completed_yuv_decode_);

  const auto format = YUVSubsamplingToMediaPixelFormat(
      decoder_->GetYUVSubsampling(), decoder_->GetYUVBitDepth());
  if (format == media::PIXEL_FORMAT_UNKNOWN)
    return;

  // In the event of a partial decode |yuv_frame_| may have been created, but
  // not populated with image data. To avoid thrashing as bytes come in, only
  // create the frame once.
  if (!yuv_frame_) {
    const auto coded_size = decoder_->DecodedYUVSize(cc::YUVIndex::kY);

    // Plane sizes are guaranteed to fit in an int32_t by
    // ImageDecoder::SetSize(); since YUV is 1 byte-per-channel, we can just
    // check width * height.
    DCHECK(coded_size.GetCheckedArea().IsValid());
    auto layout = media::VideoFrameLayout::CreateWithStrides(
        format, coded_size,
        {static_cast<int32_t>(decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kY)),
         static_cast<int32_t>(decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kU)),
         static_cast<int32_t>(
             decoder_->DecodedYUVWidthBytes(cc::YUVIndex::kV))});
    if (!layout)
      return;

    yuv_frame_ = media::VideoFrame::CreateFrameWithLayout(
        *layout, gfx::Rect(coded_size), coded_size, media::kNoTimestamp,
        /*zero_initialize_memory=*/false);
    if (!yuv_frame_)
      return;
  }

  void* planes[cc::kNumYUVPlanes] = {yuv_frame_->writable_data(0),
                                     yuv_frame_->writable_data(1),
                                     yuv_frame_->writable_data(2)};
  wtf_size_t row_bytes[cc::kNumYUVPlanes] = {
      static_cast<wtf_size_t>(yuv_frame_->stride(0)),
      static_cast<wtf_size_t>(yuv_frame_->stride(1)),
      static_cast<wtf_size_t>(yuv_frame_->stride(2))};

  // TODO(crbug.com/1073995): Add support for high bit depth format.
  const auto color_type = kGray_8_SkColorType;

  auto image_planes =
      std::make_unique<ImagePlanes>(planes, row_bytes, color_type);
  decoder_->SetImagePlanes(std::move(image_planes));
  decoder_->DecodeToYUV();
  if (decoder_->Failed() || !decoder_->HasDisplayableYUVData())
    return;

  have_completed_yuv_decode_ = true;

  gfx::ColorSpace gfx_cs;
  if (auto sk_cs = decoder_->ColorSpaceForSkImages())
    gfx_cs = gfx::ColorSpace(*sk_cs);

  const auto skyuv_cs = decoder_->GetYUVColorSpace();
  DCHECK_NE(skyuv_cs, kIdentity_SkYUVColorSpace);

  yuv_frame_->set_timestamp(GetTimestampForFrame(0));
  yuv_frame_->metadata().transformation =
      ImageOrientationToVideoTransformation(decoder_->Orientation());
  yuv_frame_->set_color_space(YUVColorSpaceToGfxColorSpace(skyuv_cs, gfx_cs));
}

base::TimeDelta ImageDecoderCore::GetTimestampForFrame(uint32_t index) const {
  // The zero entry is always populated by this point.
  DCHECK_GE(timestamp_cache_.size(), 1u);

  auto ts = decoder_->FrameTimestampAtIndex(index);
  if (ts.has_value())
    return *ts;

  if (index < timestamp_cache_.size())
    return timestamp_cache_[index];

  // Calling FrameCount() ensures duration information is populated for every
  // frame up to the current count. DecodeFrameBufferAtIndex() or DecodeToYUV()
  // have also been called this point, so index is always valid.
  DCHECK_LT(index, decoder_->FrameCount());
  DCHECK(!decoder_->Failed());

  const auto old_size = timestamp_cache_.size();
  timestamp_cache_.resize(decoder_->FrameCount());
  for (auto i = old_size; i < timestamp_cache_.size(); ++i) {
    timestamp_cache_[i] =
        timestamp_cache_[i - 1] + decoder_->FrameDurationAtIndex(i - 1);
  }

  return timestamp_cache_[index];
}

}  // namespace blink
