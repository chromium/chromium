// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"

#include <limits>
#include <stack>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_stream.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkCodecAnimation.h"
#include "third_party/skia/include/codec/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"

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

}  // anonymous namespace

SkiaImageDecoderBase::SkiaImageDecoderBase(AlphaOption alpha_option,
                                           ColorBehavior color_behavior,
                                           wtf_size_t max_decoded_bytes,
                                           wtf_size_t reading_offset)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
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
        segment_stream_ = segment_stream_ptr;
        // OnCreateSkCodec needs to read enough of the image to get the image
        // size.
        SkImageInfo image_info = codec_->getInfo();
        SetSize(static_cast<unsigned>(image_info.width()),
                static_cast<unsigned>(image_info.height()));

        return;
      }

      case SkCodec::kIncompleteInput:
        if (IsAllDataReceived()) {
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
      // SkCodec returns 0 for both still images and animated images which only
      // play once.
      if (IsAllDataReceived() && codec_->getFrameCount() == 1) {
        repetition_count_ = kAnimationNone;
        break;
      }

      repetition_count_ = kAnimationLoopOnce;
      break;
    }
    case SkCodec::kRepetitionCountInfinite:
      repetition_count_ = kAnimationLoopInfinite;
      break;
    default:
      repetition_count_ = repetition_count;
      break;
  }

  return repetition_count_;
}

bool SkiaImageDecoderBase::FrameIsReceivedAtIndex(wtf_size_t index) const {
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

wtf_size_t SkiaImageDecoderBase::DecodeFrameCount() {
  if (!codec_ || segment_stream_->IsCleared()) {
    return frame_buffer_cache_.size();
  }

  return codec_->getFrameCount();
}

void SkiaImageDecoderBase::InitializeNewFrame(wtf_size_t index) {
  DCHECK(codec_);

  ImageFrame& frame = frame_buffer_cache_[index];
  // SkCodec does not inform us if only a portion of the image was updated in
  // the current frame. Because of this, rather than correctly filling in the
  // frame rect, we set the frame rect to be the image's full size.
  // The original frame rect is not used, anyway.
  gfx::Size full_image_size = Size();
  frame.SetOriginalFrameRect(gfx::Rect(full_image_size));

  SkCodec::FrameInfo frame_info;
  bool frame_info_received = codec_->getFrameInfo(index, &frame_info);
  DCHECK(frame_info_received);
  frame.SetDuration(base::Milliseconds(frame_info.fDuration));
  wtf_size_t required_previous_frame_index;
  if (frame_info.fRequiredFrame == SkCodec::kNoFrame) {
    required_previous_frame_index = kNotFound;
  } else {
    required_previous_frame_index =
        static_cast<wtf_size_t>(frame_info.fRequiredFrame);
  }
  frame.SetRequiredPreviousFrameIndex(required_previous_frame_index);
  frame.SetDisposalMethod(ConvertDisposalMethod(frame_info.fDisposalMethod));
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

    if (!codec_ || segment_stream_->IsCleared() || IsFailedFrameIndex(current_frame_index)) {
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
        frame.AllocatePixelData(Size().width(), Size().height(),
                                ColorSpaceForSkImages());
        frame.ZeroFillPixelData();
        prior_frame_ = SkCodec::kNoFrame;
      } else {
        // We check if previous_frame_index is already initialized, meaning it
        // has been visited already, then if a viable reference frame exists.
        // If neither, decode required_previous_frame_index.
        if (previous_frame_index == kNotFound) {
          previous_frame_index = GetViableReferenceFrameIndex(current_frame_index);
          if (previous_frame_index == kNotFound) {
            frames_to_decode.push({current_frame_index, required_previous_frame_index});
            frames_to_decode.push({required_previous_frame_index, kNotFound});
            continue;
          }
        }

        if (IsFailedFrameIndex(previous_frame_index)) {
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

    if (frame.GetStatus() == ImageFrame::kFrameInitialized) {
      SkCodec::FrameInfo frame_info;
      bool frame_info_received = codec_->getFrameInfo(current_frame_index, &frame_info);
      DCHECK(frame_info_received);

      SkAlphaType alpha_type = kOpaque_SkAlphaType;
      if (frame_info.fAlphaType != kOpaque_SkAlphaType) {
        if (premultiply_alpha_) {
          alpha_type = kPremul_SkAlphaType;
        } else {
          alpha_type = kUnpremul_SkAlphaType;
        }
      }

      SkImageInfo image_info = codec_->getInfo()
                                  .makeColorType(kN32_SkColorType)
                                  .makeColorSpace(ColorSpaceForSkImages())
                                  .makeAlphaType(alpha_type);

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
    }

    SkCodec::Result incremental_decode_result = codec_->incrementalDecode();
    switch (incremental_decode_result) {
      case SkCodec::kSuccess: {
        SkCodec::FrameInfo frame_info;
        bool frame_info_received = codec_->getFrameInfo(current_frame_index, &frame_info);
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
        if (FrameIsReceivedAtIndex(current_frame_index) || IsAllDataReceived()) {
          SetFailedFrameIndex(current_frame_index);
        }
        break;
      default:
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

}  // namespace blink
