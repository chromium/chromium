// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"

#include <limits>
#include <stack>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/image-decoders/skia/segment_stream.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkCodecAnimation.h"
#include "third_party/skia/include/codec/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

ImageFrame::DisposalMethod ConvertDisposalMethod(
    SkCodecAnimation::DisposalMethod disposal_method) {
  switch (disposal_method) {
    case SkCodecAnimation::DisposalMethod::kKeep:
      return ImageFrame::kDisposeKeep;
    case SkCodecAnimation::DisposalMethod::kRestoreBGColor:
      return ImageFrame::kDisposeOverwriteBgcolor;
    case SkCodecAnimation::DisposalMethod::kRestorePrevious:
      return ImageFrame::kDisposeOverwritePrevious;
    default:
      return ImageFrame::kDisposeNotSpecified;
  }
}

ImageFrame::AlphaBlendSource ConvertAlphaBlendSource(
    SkCodecAnimation::Blend blend) {
  switch (blend) {
    case SkCodecAnimation::Blend::kSrc:
      return ImageFrame::kBlendAtopBgcolor;
    case SkCodecAnimation::Blend::kSrcOver:
      return ImageFrame::kBlendAtopPreviousFrame;
  }
  NOTREACHED();
}

}  // anonymous namespace

SkiaImageDecoderBase::SkiaImageDecoderBase(
    AlphaOption alpha_option,
    ColorBehavior color_behavior,
    wtf_size_t max_decoded_bytes,
    wtf_size_t reading_offset,
    HighBitDepthDecodingOption high_bit_depth_decoding_option)
    : ImageDecoder(alpha_option,
                   high_bit_depth_decoding_option,
                   color_behavior,
                   cc::AuxImage::kDefault,
                   max_decoded_bytes),
      reading_offset_(reading_offset) {}

SkiaImageDecoderBase::~SkiaImageDecoderBase() = default;

void SkiaImageDecoderBase::OnSetData(scoped_refptr<SegmentReader> data) {
  if (!data) {
    if (segment_stream_) {
      segment_stream_->SetReader(nullptr);
    }
    return;
  }

  if (segment_stream_) {
    DCHECK(codec_);
    segment_stream_->SetReader(std::move(data));
  } else {
    DCHECK(!codec_);

    auto segment_stream = std::make_unique<SegmentStream>(
        base::checked_cast<size_t>(reading_offset_));
    SegmentStream* segment_stream_ptr = segment_stream.get();
    segment_stream->SetReader(std::move(data));

    SkCodec::Result codec_creation_result;
    codec_ = OnCreateSkCodec(std::move(segment_stream), &codec_creation_result);

    switch (codec_creation_result) {
      case SkCodec::kSuccess: {
        // OnCreateSkCodec needs to read enough of the image to create
        // SkEncodedInfo so now is an okay time to ask the `codec_` about 1) the
        // image size and 2) the color profile.
        SkImageInfo image_info = codec_->getInfo();
        if (!SetSize(static_cast<unsigned>(image_info.width()),
                     static_cast<unsigned>(image_info.height()))) {
          codec_.reset();
          SetFailed();
          return;
        }
        if (!IgnoresColorSpace()) {
          if (const skcms_ICCProfile* profile = codec_->getICCProfile()) {
            SetEmbeddedColorProfile(std::make_unique<ColorProfile>(*profile));
          }
          if (codec_->getHdrMetadata() != skhdr::Metadata::MakeEmpty()) {
            hdr_metadata_ = gfx::HDRMetadata(codec_->getHdrMetadata());
          }
        }
        segment_stream_ = segment_stream_ptr;
        orientation_ = static_cast<ImageOrientationEnum>(codec_->getOrigin());
        return;
      }

      case SkCodec::kIncompleteInput:
        if (IsAllDataReceived()) {
          codec_.reset();
          SetFailed();
        }
        return;

      default:
        SetFailed();
        return;
    }
  }
}

int SkiaImageDecoderBase::RepetitionCount() const {
  if (!codec_ || segment_stream_->IsCleared()) {
    return repetition_count_;
  }

  DCHECK(!Failed());
  repetition_count_ = RepetitionCountInternal();
  return repetition_count_;
}

int SkiaImageDecoderBase::RepetitionCountInternal() const {
  // This value can arrive at any point in the image data stream.  Most GIFs
  // in the wild declare it near the beginning of the file, so it usually is
  // set by the time we've decoded the size, but (depending on the GIF and the
  // packets sent back by the webserver) not always.
  //
  // SkCodec will parse forward in the file if the repetition count has not been
  // seen yet.
  int repetition_count = codec_->getRepetitionCount();

  switch (repetition_count) {
    case 0: {
      // SkCodec returns 0 both for 1) still images and 2) animated images which
      // only play once.  First try to disambiguate using by checking
      // `isAnimated`.
      switch (codec_->isAnimated()) {
        case SkCodec::IsAnimated::kYes:
          return kAnimationLoopOnce;
        case SkCodec::IsAnimated::kNo:
          return kAnimationNone;
        case SkCodec::IsAnimated::kUnknown:
          break;
      }

      // Otherwise, fall back to frame-count-based heuristics.  This may end up
      // incorrectly returning `kAnimationLoopOnce` for static/single-frame
      // images that are only partially-received so far.
      if (IsAllDataReceived() && codec_->getFrameCount() == 1) {
        return kAnimationNone;
      }
      return kAnimationLoopOnce;
    }
    case SkCodec::kRepetitionCountInfinite:
      return kAnimationLoopInfinite;
    default:
      return repetition_count;
  }
}

bool SkiaImageDecoderBase::FrameIsReceivedAtIndex(wtf_size_t index) const {
  // When all input data has been received, then (by definition) it means that
  // all data for all individual frames has also been received.  (Note that the
  // default `ImageDecoder::FrameIsReceivedAtIndex` implementation just returns
  // `IsAllDataReceived()`.)
  if (IsAllDataReceived()) {
    return true;
  }

  SkCodec::FrameInfo frame_info;
  if (!codec_ || !codec_->getFrameInfo(index, &frame_info)) {
    return false;
  }
  return frame_info.fFullyReceived;
}

base::TimeDelta SkiaImageDecoderBase::FrameDurationAtIndex(
    wtf_size_t index) const {
  if (index < frame_buffer_cache_.size()) {
    return frame_buffer_cache_[index].Duration();
  }
  return base::TimeDelta();
}

bool SkiaImageDecoderBase::SetFailed() {
  segment_stream_ = nullptr;
  codec_.reset();
  return ImageDecoder::SetFailed();
}

wtf_size_t SkiaImageDecoderBase::ClearCacheExceptFrame(wtf_size_t index) {
  if (frame_buffer_cache_.size() <= 1) {
    return 0;
  }

  // SkCodec attempts to report the earliest possible required frame. But it is
  // possible that frame has been evicted. A later frame which could also
  // be used as the required frame may still be cached. Try to preserve a frame
  // that is still cached.
  wtf_size_t index2 = kNotFound;
  if (index < frame_buffer_cache_.size()) {
    const ImageFrame& frame = frame_buffer_cache_[index];
    if (frame.RequiredPreviousFrameIndex() != kNotFound &&
        (!FrameStatusSufficientForSuccessors(index) ||
         frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious)) {
      index2 = GetViableReferenceFrameIndex(index);
    }
  }

  return ClearCacheExceptTwoFrames(index, index2);
}

bool SkiaImageDecoderBase::ImageIsHighBitDepth() {
  if (codec_) {
    return codec_->hasHighBitDepthEncodedData();
  }

  return false;
}

wtf_size_t SkiaImageDecoderBase::DecodeFrameCount() {
  if (!codec_ || segment_stream_->IsCleared()) {
    return frame_buffer_cache_.size();
  }

  return codec_->getFrameCount();
}

void SkiaImageDecoderBase::InitializeNewFrame(wtf_size_t index) {
  DCHECK(codec_);

  SkCodec::FrameInfo frame_info;
  bool frame_info_received = codec_->getFrameInfo(index, &frame_info);
  DCHECK(frame_info_received);

  ImageFrame& frame = frame_buffer_cache_[index];
  frame.SetDuration(base::Milliseconds(frame_info.fDuration));
  wtf_size_t required_previous_frame_index;
  if (frame_info.fRequiredFrame == SkCodec::kNoFrame) {
    required_previous_frame_index = kNotFound;
  } else {
    required_previous_frame_index =
        static_cast<wtf_size_t>(frame_info.fRequiredFrame);
  }
  frame.SetOriginalFrameRect(gfx::SkIRectToRect(frame_info.fFrameRect));
  frame.SetRequiredPreviousFrameIndex(required_previous_frame_index);
  frame.SetDisposalMethod(ConvertDisposalMethod(frame_info.fDisposalMethod));
  frame.SetAlphaBlendSource(ConvertAlphaBlendSource(frame_info.fBlend));

  if (high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat &&
      ImageIsHighBitDepth()) {
    frame.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);
  } else {
    frame.SetPixelFormat(ImageFrame::PixelFormat::kN32);
  }
}

void SkiaImageDecoderBase::Decode(wtf_size_t index) {
  struct FrameData {
    wtf_size_t index;
    wtf_size_t previous_frame_index;
  };
  std::stack<FrameData> frames_to_decode;
  frames_to_decode.push({index, kNotFound});

  while (!frames_to_decode.empty()) {
    const FrameData& current_frame = frames_to_decode.top();
    wtf_size_t current_frame_index = current_frame.index;
    wtf_size_t previous_frame_index = current_frame.previous_frame_index;
    frames_to_decode.pop();

    if (!codec_ || segment_stream_->IsCleared() ||
        IsFailedFrameIndex(current_frame_index)) {
      continue;
    }

    DCHECK(!Failed());

    DCHECK_LT(current_frame_index, frame_buffer_cache_.size());

    ImageFrame& frame = frame_buffer_cache_[current_frame_index];
    if (frame.GetStatus() == ImageFrame::kFrameComplete) {
      continue;
    }

    UpdateAggressivePurging(current_frame_index);

    if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
      wtf_size_t required_previous_frame_index =
          frame.RequiredPreviousFrameIndex();
      if (required_previous_frame_index == kNotFound) {
        if (!frame.AllocatePixelData(Size().width(), Size().height(),
                                     ColorSpaceForSkImages())) {
          SetFailedFrameIndex(current_frame_index);
          continue;
        }
        frame.ZeroFillPixelData();
        prior_frame_ = SkCodec::kNoFrame;
      } else {
        // We check if previous_frame_index is already initialized, meaning it
        // has been visited already, then if a viable reference frame exists.
        // If neither, decode required_previous_frame_index.
        if (previous_frame_index == kNotFound) {
          previous_frame_index =
              GetViableReferenceFrameIndex(current_frame_index);
          if (previous_frame_index == kNotFound) {
            frames_to_decode.push(
                {current_frame_index, required_previous_frame_index});
            frames_to_decode.push({required_previous_frame_index, kNotFound});
            continue;
          }
        }

        if (IsFailedFrameIndex(previous_frame_index)) {
          SetFailedFrameIndex(current_frame_index);
          continue;
        }

        // We try to reuse |previous_frame| as starting state to avoid copying.
        // If CanReusePreviousFrameBuffer returns false, we must copy the data
        // since |previous_frame| is necessary to decode this or later frames.
        // In that case copy the data instead.
        ImageFrame& previous_frame = frame_buffer_cache_[previous_frame_index];
        if ((!CanReusePreviousFrameBuffer(current_frame_index) ||
             !frame.TakeBitmapDataIfWritable(&previous_frame)) &&
            !frame.CopyBitmapData(previous_frame)) {
          SetFailedFrameIndex(current_frame_index);
          continue;
        }
        prior_frame_ = previous_frame_index;
      }
    }

    bool already_started_current_frame =
        already_started_frame_.has_value() &&
        already_started_frame_.value() == current_frame_index;
    if (!already_started_current_frame) {
      // `kFrameEmpty` and `kFrameComplete` are handled above.
      // `kFrameInitialized` is possible when decoding a frame from scratch.
      // `kFramePartial` is possible when resuming to decode a frame that
      // previously returned `kIncompleteInput` from `incrementalDecode`.
      DCHECK(frame.GetStatus() == ImageFrame::kFrameInitialized ||
             frame.GetStatus() == ImageFrame::kFramePartial);

      SkCodec::FrameInfo frame_info;
      bool frame_info_received =
          codec_->getFrameInfo(current_frame_index, &frame_info);
      DCHECK(frame_info_received);

      SkAlphaType alpha_type = kOpaque_SkAlphaType;
      if (frame_info.fAlphaType != kOpaque_SkAlphaType) {
        if (premultiply_alpha_) {
          alpha_type = kPremul_SkAlphaType;
        } else {
          alpha_type = kUnpremul_SkAlphaType;
        }
      }

      SkColorType color_type = kUnknown_SkColorType;
      switch (frame.GetPixelFormat()) {
        case ImageFrame::PixelFormat::kRGBA_F16:
          color_type = kRGBA_F16_SkColorType;
          break;
        case ImageFrame::PixelFormat::kN32:
          color_type = kN32_SkColorType;
          break;
      }
      DCHECK_NE(color_type, kUnknown_SkColorType);

      sk_sp<SkColorSpace> color_space;
      if (const ColorProfileTransform* transform = ColorTransform()) {
        const skcms_ICCProfile* dst_profile = transform->DstProfile();
        DCHECK(dst_profile);  // Always non-null ptr to `dst_profile_` field.
        color_space = SkColorSpace::Make(*dst_profile);
      } else {
        // Explicitly ask for no color transformation.  This avoids transforming
        // into sRGB if/when `SkEncodedInfo::makeImageInfo` has set
        // `codec_->getInfo().colorSpace()` to sRGB as a fallback.
        color_space = nullptr;
      }

      SkImageInfo image_info = codec_->getInfo()
                                   .makeColorType(color_type)
                                   .makeAlphaType(alpha_type)
                                   .makeColorSpace(color_space);

      SkCodec::Options options;
      options.fFrameIndex = current_frame_index;
      options.fPriorFrame = prior_frame_;
      options.fZeroInitialized = SkCodec::kNo_ZeroInitialized;

      SkCodec::Result start_incremental_decode_result =
          codec_->startIncrementalDecode(image_info, frame.Bitmap().getPixels(),
                                         frame.Bitmap().rowBytes(), &options);
      switch (start_incremental_decode_result) {
        case SkCodec::kSuccess:
          break;
        case SkCodec::kIncompleteInput:
          continue;
        default:
          SetFailedFrameIndex(current_frame_index);
          continue;
      }
      frame.SetStatus(ImageFrame::kFramePartial);
      already_started_frame_.emplace(current_frame_index);
    }

    SkCodec::Result incremental_decode_result = codec_->incrementalDecode();
    switch (incremental_decode_result) {
      case SkCodec::kSuccess: {
        already_started_frame_.reset();
        SkCodec::FrameInfo frame_info;
        bool frame_info_received =
            codec_->getFrameInfo(current_frame_index, &frame_info);
        DCHECK(frame_info_received);
        frame.SetHasAlpha(frame_info.fAlphaType !=
                          SkAlphaType::kOpaque_SkAlphaType);
        frame.SetPixelsChanged(true);
        frame.SetStatus(ImageFrame::kFrameComplete);
        PostDecodeProcessing(current_frame_index);
        break;
      }
      case SkCodec::kIncompleteInput:
        frame.SetPixelsChanged(true);
        if (FrameIsReceivedAtIndex(current_frame_index)) {
          SetFailedFrameIndex(current_frame_index);
        }
        break;
      default:
        already_started_frame_.reset();
        frame.SetPixelsChanged(true);
        SetFailedFrameIndex(current_frame_index);
        break;
    }
  }
}

bool SkiaImageDecoderBase::CanReusePreviousFrameBuffer(
    wtf_size_t frame_index) const {
  DCHECK_LT(frame_index, frame_buffer_cache_.size());
  return frame_buffer_cache_[frame_index].GetDisposalMethod() !=
         ImageFrame::kDisposeOverwritePrevious;
}

wtf_size_t SkiaImageDecoderBase::GetViableReferenceFrameIndex(
    wtf_size_t dependent_index) const {
  DCHECK_LT(dependent_index, frame_buffer_cache_.size());

  wtf_size_t required_previous_frame_index =
      frame_buffer_cache_[dependent_index].RequiredPreviousFrameIndex();

  // Any frame in the range [|required_previous_frame_index|, |dependent_index|)
  // which has a disposal method other than kRestorePrevious can be provided as
  // the prior frame to SkCodec.
  //
  // SkCodec sets SkCodec::FrameInfo::fRequiredFrame to the earliest frame which
  // can be used. This might come up when several frames update the same
  // subregion. If that same subregion is about to be overwritten, it doesn't
  // matter which frame in that chain is provided.
  DCHECK_NE(required_previous_frame_index, kNotFound);
  // Loop backwards because the frames most likely to be in cache are the most
  // recent.
  for (wtf_size_t i = dependent_index - 1; i != required_previous_frame_index;
       i--) {
    const ImageFrame& frame = frame_buffer_cache_[i];

    if (frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious) {
      continue;
    }

    if (frame.GetStatus() == ImageFrame::kFrameComplete) {
      return i;
    }
  }

  return kNotFound;
}

void SkiaImageDecoderBase::SetFailedFrameIndex(wtf_size_t index) {
  decode_failed_frames_.insert(index);
  if (decode_failed_frames_.size() == DecodeFrameCount()) {
    SetFailed();
  }
}

bool SkiaImageDecoderBase::IsFailedFrameIndex(wtf_size_t index) const {
  return decode_failed_frames_.contains(index);
}

bool SkiaImageDecoderBase::SetSize(unsigned width, unsigned height) {
  DCHECK(!IsDecodedSizeAvailable());
  // Protect against large images. See http://bugzil.la/251381 for more details.
  // The limit of `1000000` has been copied from `blink::PNGImageDecoder` and
  // originates all the way back in WebKit.
  const uint32_t kMaxSize = 1000000;
  return (width <= kMaxSize) && (height <= kMaxSize) &&
         ImageDecoder::SetSize(width, height);
}

}  // namespace blink
