/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/webp/webp_image_decoder.h"

#include <string.h>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace {

// Returns two point ranges (<left, width> pairs) at row |canvasY| which belong
// to |src| but not |dst|. A range is empty if its width is 0.
inline void findBlendRangeAtRow(const blink::IntRect& src,
                                const blink::IntRect& dst,
                                int canvasY,
                                int& left1,
                                int& width1,
                                int& left2,
                                int& width2) {
  SECURITY_DCHECK(canvasY >= src.Y() && canvasY < src.MaxY());
  left1 = -1;
  width1 = 0;
  left2 = -1;
  width2 = 0;

  if (canvasY < dst.Y() || canvasY >= dst.MaxY() || src.X() >= dst.MaxX() ||
      src.MaxX() <= dst.X()) {
    left1 = src.X();
    width1 = src.Width();
    return;
  }

  if (src.X() < dst.X()) {
    left1 = src.X();
    width1 = dst.X() - src.X();
  }

  if (src.MaxX() > dst.MaxX()) {
    left2 = dst.MaxX();
    width2 = src.MaxX() - dst.MaxX();
  }
}

// alphaBlendPremultiplied and alphaBlendNonPremultiplied are separate methods,
// even though they only differ by one line. This is done so that the compiler
// can inline BlendSrcOverDstPremultiplied() and BlensSrcOverDstRaw() calls.
// For GIF images, this optimization reduces decoding time by 15% for 3MB
// images.
void alphaBlendPremultiplied(blink::ImageFrame& src,
                             blink::ImageFrame& dst,
                             int canvasY,
                             int left,
                             int width) {
  for (int x = 0; x < width; ++x) {
    int canvasX = left + x;
    blink::ImageFrame::PixelData* pixel = src.GetAddr(canvasX, canvasY);
    if (SkGetPackedA32(*pixel) != 0xff) {
      blink::ImageFrame::PixelData prevPixel = *dst.GetAddr(canvasX, canvasY);
      blink::ImageFrame::BlendSrcOverDstPremultiplied(pixel, prevPixel);
    }
  }
}

void alphaBlendNonPremultiplied(blink::ImageFrame& src,
                                blink::ImageFrame& dst,
                                int canvasY,
                                int left,
                                int width) {
  for (int x = 0; x < width; ++x) {
    int canvasX = left + x;
    blink::ImageFrame::PixelData* pixel = src.GetAddr(canvasX, canvasY);
    if (SkGetPackedA32(*pixel) != 0xff) {
      blink::ImageFrame::PixelData prevPixel = *dst.GetAddr(canvasX, canvasY);
      blink::ImageFrame::BlendSrcOverDstRaw(pixel, prevPixel);
    }
  }
}

// Do not rename entries nor reuse numeric values. See the following link for
// descriptions: https://developers.google.com/speed/webp/docs/riff_container.
enum WebPFileFormat {
  kSimpleLossyFileFormat = 0,
  kSimpleLosslessFileFormat = 1,
  kExtendedAlphaFileFormat = 2,
  kExtendedAnimationFileFormat = 3,
  kExtendedAnimationWithAlphaFileFormat = 4,
  kUnknownFileFormat = 5,
  kCountWebPFileFormats
};

// Validates that |blob| is a simple lossy WebP image. Note that this explicitly
// checks "WEBPVP8 " to exclude extended lossy WebPs that don't actually use any
// extended features.
//
// TODO(crbug.com/1009237): consider combining this with the logic to detect
// WebPs that can be decoded to YUV.
bool IsSimpleLossyWebPImage(const sk_sp<SkData>& blob) {
  if (blob->size() < 20UL)
    return false;
  DCHECK(blob->bytes());
  return !memcmp(blob->bytes(), "RIFF", 4) &&
         !memcmp(blob->bytes() + 8UL, "WEBPVP8 ", 8);
}

// This method parses |blob|'s header and emits a UMA with the file format, as
// defined by WebP, see WebPFileFormat.
void UpdateWebPFileFormatUMA(const sk_sp<SkData>& blob) {
  if (!IsMainThread())
    return;

  WebPBitstreamFeatures features{};
  if (WebPGetFeatures(blob->bytes(), blob->size(), &features) != VP8_STATUS_OK)
    return;

  // These constants are defined verbatim in
  // webp_dec.c::ParseHeadersInternal().
  constexpr int kLossyFormat = 1;
  constexpr int kLosslessFormat = 2;

  WebPFileFormat file_format = kUnknownFileFormat;
  if (features.has_alpha && features.has_animation)
    file_format = kExtendedAnimationWithAlphaFileFormat;
  else if (features.has_animation)
    file_format = kExtendedAnimationFileFormat;
  else if (features.has_alpha)
    file_format = kExtendedAlphaFileFormat;
  else if (features.format == kLossyFormat)
    file_format = kSimpleLossyFileFormat;
  else if (features.format == kLosslessFormat)
    file_format = kSimpleLosslessFileFormat;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      blink::EnumerationHistogram, file_format_histogram,
      ("Blink.DecodedImage.WebPFileFormat", kCountWebPFileFormats));
  file_format_histogram.Count(file_format);
}

}  // namespace

namespace blink {

WEBPImageDecoder::WEBPImageDecoder(AlphaOption alpha_option,
                                   const ColorBehavior& color_behavior,
                                   size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   max_decoded_bytes),
      decoder_(nullptr),
      format_flags_(0),
      frame_background_has_alpha_(false),
      demux_(nullptr),
      demux_state_(WEBP_DEMUX_PARSING_HEADER),
      have_already_parsed_this_data_(false),
      repetition_count_(kAnimationLoopOnce),
      decoded_height_(0) {
  blend_function_ = (alpha_option == kAlphaPremultiplied)
                        ? alphaBlendPremultiplied
                        : alphaBlendNonPremultiplied;
}

WEBPImageDecoder::~WEBPImageDecoder() {
  Clear();
}

void WEBPImageDecoder::Clear() {
  WebPDemuxDelete(demux_);
  demux_ = nullptr;
  consolidated_data_.reset();
  ClearDecoder();
}

void WEBPImageDecoder::ClearDecoder() {
  WebPIDelete(decoder_);
  decoder_ = nullptr;
  decoded_height_ = 0;
  frame_background_has_alpha_ = false;
}

WEBP_CSP_MODE WEBPImageDecoder::RGBOutputMode() {
  DCHECK(!IsDoingYuvDecode());
  if (ColorTransform()) {
    // Swizzling between RGBA and BGRA is zero cost in a color transform.
    // So when we have a color transform, we should decode to whatever is
    // easiest for libwebp, and then let the color transform swizzle if
    // necessary.
    // Lossy webp is encoded as YUV (so RGBA and BGRA are the same cost).
    // Lossless webp is encoded as BGRA. This means decoding to BGRA is
    // either faster or the same cost as RGBA.
    return MODE_BGRA;
  }
  bool premultiply = (format_flags_ & ALPHA_FLAG) && premultiply_alpha_;
#if SK_B32_SHIFT  // Output little-endian RGBA pixels (Android)
  return premultiply ? MODE_rgbA : MODE_RGBA;
#else  // Output little-endian BGRA pixels.
  return premultiply ? MODE_bgrA : MODE_BGRA;
#endif
}

bool WEBPImageDecoder::CanAllowYUVDecodingForWebP() {
  if (!consolidated_data_)
    return false;
  // Should have been updated with a recent call to UpdateDemuxer().
  WebPBitstreamFeatures features;
  if (RuntimeEnabledFeatures::DecodeLossyWebPImagesToYUVEnabled() &&
      (demux_state_ == WEBP_DEMUX_PARSED_HEADER ||
       demux_state_ == WEBP_DEMUX_DONE) &&
      WebPGetFeatures(consolidated_data_->bytes(), consolidated_data_->size(),
                      &features) == VP8_STATUS_OK) {
    bool is_animated = !!(format_flags_ & ANIMATION_FLAG);
    constexpr int kLossyFormat = ImageDecoder::CompressionFormat::kLossyFormat;
    // TODO(crbug/910276): Change after alpha support.
    if (features.format != kLossyFormat || features.has_alpha || is_animated)
      return false;

    // TODO(crbug/911246): Stop vetoing images with ICCP after Skia supports
    // transforming colorspace within YUV, which would allow colorspace
    // conversion during decode. Alternatively, look into passing along
    // transform for raster-time.
    bool has_iccp = !!(format_flags_ & ICCP_FLAG);
    return !has_iccp;
  }
  return false;
}

void WEBPImageDecoder::OnSetData(SegmentReader* data) {
  have_already_parsed_this_data_ = false;
  // TODO(crbug.com/943519): Modify this approach for incremental YUV (when
  // we don't require IsAllDataReceived() to be true before decoding).
  if (IsAllDataReceived()) {
    UpdateDemuxer();
    allow_decode_to_yuv_ =
        RuntimeEnabledFeatures::DecodeLossyWebPImagesToYUVEnabled() &&
        CanAllowYUVDecodingForWebP();
  }
}

int WEBPImageDecoder::RepetitionCount() const {
  return Failed() ? kAnimationLoopOnce : repetition_count_;
}

bool WEBPImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (!demux_ || demux_state_ <= WEBP_DEMUX_PARSING_HEADER)
    return false;
  if (!(format_flags_ & ANIMATION_FLAG))
    return ImageDecoder::FrameIsReceivedAtIndex(index);
  bool frame_is_received_at_index = index < frame_buffer_cache_.size();
  return frame_is_received_at_index;
}

base::TimeDelta WEBPImageDecoder::FrameDurationAtIndex(size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Duration()
             : base::TimeDelta();
}

bool WEBPImageDecoder::UpdateDemuxer() {
  if (Failed())
    return false;

  const unsigned kWebpHeaderSize = 30;
  if (data_->size() < kWebpHeaderSize)
    return IsAllDataReceived() ? SetFailed() : false;

  if (have_already_parsed_this_data_)
    return true;

  have_already_parsed_this_data_ = true;

  if (consolidated_data_ && consolidated_data_->size() >= data_->size()) {
    // Less data provided than last time. |consolidated_data_| is guaranteed
    // to be its own copy of the data, so it is safe to keep it.
    return true;
  }

  if (IsAllDataReceived() && !consolidated_data_) {
    consolidated_data_ = data_->GetAsSkData();
  } else {
    buffer_.ReserveCapacity(data_->size());
    while (buffer_.size() < data_->size()) {
      const char* segment;
      const size_t bytes = data_->GetSomeData(segment, buffer_.size());
      DCHECK(bytes);
      buffer_.Append(segment, bytes);
    }
    DCHECK_EQ(buffer_.size(), data_->size());
    consolidated_data_ =
        SkData::MakeWithoutCopy(buffer_.data(), buffer_.size());
  }

  WebPDemuxDelete(demux_);
  WebPData input_data = {
      reinterpret_cast<const uint8_t*>(consolidated_data_->data()),
      consolidated_data_->size()};
  demux_ = WebPDemuxPartial(&input_data, &demux_state_);
  if (!demux_ || (IsAllDataReceived() && demux_state_ != WEBP_DEMUX_DONE)) {
    if (!demux_)
      consolidated_data_.reset();
    return SetFailed();
  }

  DCHECK_GT(demux_state_, WEBP_DEMUX_PARSING_HEADER);
  if (!WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT))
    return false;  // Wait until the encoded image frame data arrives.

  if (!IsDecodedSizeAvailable()) {
    uint32_t width = WebPDemuxGetI(demux_, WEBP_FF_CANVAS_WIDTH);
    uint32_t height = WebPDemuxGetI(demux_, WEBP_FF_CANVAS_HEIGHT);
    if (!SetSize(base::strict_cast<unsigned>(width),
                 base::strict_cast<unsigned>(height)))
      return SetFailed();

    UpdateWebPFileFormatUMA(consolidated_data_);

    format_flags_ = WebPDemuxGetI(demux_, WEBP_FF_FORMAT_FLAGS);
    if (!(format_flags_ & ANIMATION_FLAG)) {
      repetition_count_ = kAnimationNone;
    } else {
      // Since we have parsed at least one frame, even if partially,
      // the global animation (ANIM) properties have been read since
      // an ANIM chunk must precede the ANMF frame chunks.
      repetition_count_ = WebPDemuxGetI(demux_, WEBP_FF_LOOP_COUNT);
      // Repetition count is always <= 16 bits.
      DCHECK_EQ(repetition_count_, repetition_count_ & 0xffff);
      // Repetition count is treated as n + 1 cycles for GIF. WebP defines loop
      // count as the number of cycles, with 0 meaning infinite.
      repetition_count_ = repetition_count_ == 0 ? kAnimationLoopInfinite
                                                 : repetition_count_ - 1;
      // FIXME: Implement ICC profile support for animated images.
      format_flags_ &= ~ICCP_FLAG;
    }

    if ((format_flags_ & ICCP_FLAG) && !IgnoresColorSpace())
      ReadColorProfile();
  }

  DCHECK(IsDecodedSizeAvailable());

  size_t frame_count = WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT);
  UpdateAggressivePurging(frame_count);

  return true;
}

void WEBPImageDecoder::OnInitFrameBuffer(size_t frame_index) {
  // ImageDecoder::InitFrameBuffer does a DCHECK if |frame_index| exists.
  ImageFrame& buffer = frame_buffer_cache_[frame_index];

  const size_t required_previous_frame_index =
      buffer.RequiredPreviousFrameIndex();
  if (required_previous_frame_index == kNotFound) {
    frame_background_has_alpha_ =
        !buffer.OriginalFrameRect().Contains(IntRect(IntPoint(), Size()));
  } else {
    const ImageFrame& prev_buffer =
        frame_buffer_cache_[required_previous_frame_index];
    frame_background_has_alpha_ =
        prev_buffer.HasAlpha() || (prev_buffer.GetDisposalMethod() ==
                                   ImageFrame::kDisposeOverwriteBgcolor);
  }

  // The buffer is transparent outside the decoded area while the image is
  // loading. The correct alpha value for the frame will be set when it is fully
  // decoded.
  buffer.SetHasAlpha(true);
}

void WEBPImageDecoder::DecodeToYUV() {
  DCHECK(IsDoingYuvDecode());

  if (Failed())
    return;

  DCHECK(demux_);
  DCHECK(!(format_flags_ & ANIMATION_FLAG));

  WebPIterator webp_iter;
  // libwebp is 1-indexed.
  if (!WebPDemuxGetFrame(demux_, 1 /* frame */, &webp_iter)) {
    SetFailed();
  } else {
    std::unique_ptr<WebPIterator, void (*)(WebPIterator*)> webp_frame(
        &webp_iter, WebPDemuxReleaseIterator);
    DecodeSingleFrameToYUV(webp_frame->fragment.bytes,
                           webp_frame->fragment.size);
  }
}

IntSize WEBPImageDecoder::DecodedYUVSize(int component) const {
  DCHECK_GE(component, 0);
  // TODO(crbug.com/910276): Change after alpha support.
  DCHECK_LE(component, 2);
  DCHECK(IsDecodedSizeAvailable());
  switch (component) {
    case SkYUVAIndex::kY_Index:
      return Size();
    case SkYUVAIndex::kU_Index:
      FALLTHROUGH;
    case SkYUVAIndex::kV_Index:
      return IntSize((Size().Width() + 1) / 2, (Size().Height() + 1) / 2);
  }
  NOTREACHED();
  return IntSize(0, 0);
}

size_t WEBPImageDecoder::DecodedYUVWidthBytes(int component) const {
  DCHECK_GE(component, 0);
  DCHECK_LE(component, 2);
  switch (component) {
    case SkYUVAIndex::kY_Index:
      return base::checked_cast<size_t>(Size().Width());
    case SkYUVAIndex::kU_Index:
      FALLTHROUGH;
    case SkYUVAIndex::kV_Index:
      return base::checked_cast<size_t>((Size().Width() + 1) / 2);
  }
  NOTREACHED();
  return 0;
}

SkYUVColorSpace WEBPImageDecoder::GetYUVColorSpace() const {
  return SkYUVColorSpace::kRec601_SkYUVColorSpace;
}

cc::YUVSubsampling WEBPImageDecoder::GetYUVSubsampling() const {
  DCHECK(consolidated_data_);
  if (IsSimpleLossyWebPImage(consolidated_data_))
    return cc::YUVSubsampling::k420;
  // It is possible for a non-simple lossy WebP to also be YUV 4:2:0. However,
  // we're being conservative here because this is currently only used for
  // hardware decode acceleration, and WebPs other than simple lossy are not
  // supported in that path anyway.
  return cc::YUVSubsampling::kUnknown;
}

bool WEBPImageDecoder::CanReusePreviousFrameBuffer(size_t frame_index) const {
  DCHECK(frame_index < frame_buffer_cache_.size());
  return frame_buffer_cache_[frame_index].GetAlphaBlendSource() !=
         ImageFrame::kBlendAtopPreviousFrame;
}

void WEBPImageDecoder::ClearFrameBuffer(size_t frame_index) {
  if (demux_ && demux_state_ >= WEBP_DEMUX_PARSED_HEADER &&
      frame_buffer_cache_[frame_index].GetStatus() ==
          ImageFrame::kFramePartial) {
    // Clear the decoder state so that this partial frame can be decoded again
    // when requested.
    ClearDecoder();
  }
  ImageDecoder::ClearFrameBuffer(frame_index);
}

void WEBPImageDecoder::ReadColorProfile() {
  WebPChunkIterator chunk_iterator;
  if (!WebPDemuxGetChunk(demux_, "ICCP", 1, &chunk_iterator)) {
    WebPDemuxReleaseChunkIterator(&chunk_iterator);
    return;
  }

  const char* profile_data =
      reinterpret_cast<const char*>(chunk_iterator.chunk.bytes);
  size_t profile_size = chunk_iterator.chunk.size;

  if (auto profile = ColorProfile::Create(profile_data, profile_size)) {
    if (profile->GetProfile()->data_color_space == skcms_Signature_RGB) {
      SetEmbeddedColorProfile(std::move(profile));
    }
  } else {
    DLOG(ERROR) << "Failed to parse image ICC profile";
  }

  WebPDemuxReleaseChunkIterator(&chunk_iterator);
}

void WEBPImageDecoder::ApplyPostProcessing(size_t frame_index) {
  ImageFrame& buffer = frame_buffer_cache_[frame_index];
  int width;
  int decoded_height;
  // TODO(crbug.com/911246): Do post-processing once skcms_Transform
  // supports multiplanar formats.
  DCHECK(!IsDoingYuvDecode());

  if (!WebPIDecGetRGB(decoder_, &decoded_height, &width, nullptr, nullptr))
    return;  // See also https://bugs.webkit.org/show_bug.cgi?id=74062
  if (decoded_height <= 0)
    return;

  const IntRect& frame_rect = buffer.OriginalFrameRect();
  SECURITY_DCHECK(width == frame_rect.Width());
  SECURITY_DCHECK(decoded_height <= frame_rect.Height());
  const int left = frame_rect.X();
  const int top = frame_rect.Y();

  // TODO (msarett):
  // Here we apply the color space transformation to the dst space.
  // It does not really make sense to transform to a gamma-encoded
  // space and then immediately after, perform a linear premultiply
  // and linear blending.  Can we find a way to perform the
  // premultiplication and blending in a linear space?
  ColorProfileTransform* xform = ColorTransform();
  if (xform) {
    skcms_PixelFormat kSrcFormat = skcms_PixelFormat_BGRA_8888;
    skcms_PixelFormat kDstFormat = skcms_PixelFormat_RGBA_8888;
    skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
    for (int y = decoded_height_; y < decoded_height; ++y) {
      const int canvas_y = top + y;
      uint8_t* row = reinterpret_cast<uint8_t*>(buffer.GetAddr(left, canvas_y));
      bool color_conversion_successful = skcms_Transform(
          row, kSrcFormat, alpha_format, xform->SrcProfile(), row, kDstFormat,
          alpha_format, xform->DstProfile(), width);
      DCHECK(color_conversion_successful);
      uint8_t* pixel = row;
      for (int x = 0; x < width; ++x, pixel += 4) {
        const int canvas_x = left + x;
        buffer.SetRGBA(canvas_x, canvas_y, pixel[0], pixel[1], pixel[2],
                       pixel[3]);
      }
    }
  }

  // During the decoding of the current frame, we may have set some pixels to be
  // transparent (i.e. alpha < 255). If the alpha blend source was
  // 'BlendAtopPreviousFrame', the values of these pixels should be
  // determined by blending them against the pixels of the corresponding
  // previous frame. Compute the correct opaque values now.
  // FIXME: This could be avoided if libwebp decoder had an API that used the
  // previous required frame to do the alpha-blending by itself.
  if ((format_flags_ & ANIMATION_FLAG) && frame_index &&
      buffer.GetAlphaBlendSource() == ImageFrame::kBlendAtopPreviousFrame &&
      buffer.RequiredPreviousFrameIndex() != kNotFound) {
    ImageFrame& prev_buffer = frame_buffer_cache_[frame_index - 1];
    DCHECK_EQ(prev_buffer.GetStatus(), ImageFrame::kFrameComplete);
    ImageFrame::DisposalMethod prev_disposal_method =
        prev_buffer.GetDisposalMethod();
    if (prev_disposal_method == ImageFrame::kDisposeKeep) {
      // Blend transparent pixels with pixels in previous canvas.
      for (int y = decoded_height_; y < decoded_height; ++y) {
        blend_function_(buffer, prev_buffer, top + y, left, width);
      }
    } else if (prev_disposal_method == ImageFrame::kDisposeOverwriteBgcolor) {
      const IntRect& prev_rect = prev_buffer.OriginalFrameRect();
      // We need to blend a transparent pixel with the starting value (from just
      // after the InitFrame() call). If the pixel belongs to prev_rect, the
      // starting value was fully transparent, so this is a no-op. Otherwise, we
      // need to blend against the pixel from the previous canvas.
      for (int y = decoded_height_; y < decoded_height; ++y) {
        int canvas_y = top + y;
        int left1, width1, left2, width2;
        findBlendRangeAtRow(frame_rect, prev_rect, canvas_y, left1, width1,
                            left2, width2);
        if (width1 > 0)
          blend_function_(buffer, prev_buffer, canvas_y, left1, width1);
        if (width2 > 0)
          blend_function_(buffer, prev_buffer, canvas_y, left2, width2);
      }
    }
  }

  decoded_height_ = decoded_height;
  buffer.SetPixelsChanged(true);
}

size_t WEBPImageDecoder::DecodeFrameCount() {
  // If UpdateDemuxer() fails, return the existing number of frames. This way if
  // we get halfway through the image before decoding fails, we won't suddenly
  // start reporting that the image has zero frames.
  return UpdateDemuxer() ? WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT)
                         : frame_buffer_cache_.size();
}

void WEBPImageDecoder::InitializeNewFrame(size_t index) {
  if (!(format_flags_ & ANIMATION_FLAG)) {
    DCHECK(!index);
    return;
  }
  WebPIterator animated_frame;
  WebPDemuxGetFrame(demux_, index + 1, &animated_frame);
  DCHECK_EQ(animated_frame.complete, 1);
  ImageFrame* buffer = &frame_buffer_cache_[index];
  IntRect frame_rect(animated_frame.x_offset, animated_frame.y_offset,
                     animated_frame.width, animated_frame.height);
  buffer->SetOriginalFrameRect(
      Intersection(frame_rect, IntRect(IntPoint(), Size())));
  buffer->SetDuration(
      base::TimeDelta::FromMilliseconds(animated_frame.duration));
  buffer->SetDisposalMethod(animated_frame.dispose_method ==
                                    WEBP_MUX_DISPOSE_BACKGROUND
                                ? ImageFrame::kDisposeOverwriteBgcolor
                                : ImageFrame::kDisposeKeep);
  buffer->SetAlphaBlendSource(animated_frame.blend_method == WEBP_MUX_BLEND
                                  ? ImageFrame::kBlendAtopPreviousFrame
                                  : ImageFrame::kBlendAtopBgcolor);
  buffer->SetRequiredPreviousFrameIndex(
      FindRequiredPreviousFrame(index, !animated_frame.has_alpha));
  WebPDemuxReleaseIterator(&animated_frame);
}

void WEBPImageDecoder::Decode(size_t index) {
  DCHECK(!IsDoingYuvDecode());

  if (Failed())
    return;

  Vector<size_t> frames_to_decode = FindFramesToDecode(index);

  DCHECK(demux_);
  for (auto i = frames_to_decode.rbegin(); i != frames_to_decode.rend(); ++i) {
    if ((format_flags_ & ANIMATION_FLAG) && !InitFrameBuffer(*i)) {
      SetFailed();
      return;
    }

    WebPIterator webp_iter;
    if (!WebPDemuxGetFrame(demux_, *i + 1, &webp_iter)) {
      SetFailed();
    } else {
      std::unique_ptr<WebPIterator, void (*)(WebPIterator*)> webp_frame(
          &webp_iter, WebPDemuxReleaseIterator);
      DecodeSingleFrame(webp_frame->fragment.bytes, webp_frame->fragment.size,
                        *i);
    }

    if (Failed())
      return;

    // If this returns false, we need more data to continue decoding.
    if (!PostDecodeProcessing(*i))
      break;
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= frame_buffer_cache_.size() - 1 && IsAllDataReceived() &&
      demux_ && demux_state_ != WEBP_DEMUX_DONE)
    SetFailed();
}

bool WEBPImageDecoder::DecodeSingleFrameToYUV(const uint8_t* data_bytes,
                                              size_t data_size) {
  DCHECK(IsDoingYuvDecode());
  DCHECK(!Failed());

  bool size_available_after_init = IsSizeAvailable();
  DCHECK(size_available_after_init);

  // Set up decoder_buffer_ with output mode
  if (!decoder_) {
    WebPInitDecBuffer(&decoder_buffer_);
    decoder_buffer_.colorspace = MODE_YUV;  // TODO(crbug.com/910276): Change
                                            // after alpha YUV support is added.
  }

  ImagePlanes* image_planes = image_planes_.get();
  DCHECK(image_planes);
  // Even if |decoder_| already exists, we must get most up-to-date pointers
  // because memory location might change e.g. upon tab resume.
  decoder_buffer_.u.YUVA.y =
      static_cast<uint8_t*>(image_planes->Plane(SkYUVAIndex::kY_Index));
  decoder_buffer_.u.YUVA.u =
      static_cast<uint8_t*>(image_planes->Plane(SkYUVAIndex::kU_Index));
  decoder_buffer_.u.YUVA.v =
      static_cast<uint8_t*>(image_planes->Plane(SkYUVAIndex::kV_Index));

  if (!decoder_) {
    // libwebp only supports YUV 420 subsampling
    decoder_buffer_.u.YUVA.y_stride =
        image_planes->RowBytes(SkYUVAIndex::kY_Index);
    decoder_buffer_.u.YUVA.y_size =
        decoder_buffer_.u.YUVA.y_stride *
        DecodedYUVSize(SkYUVAIndex::kY_Index).Height();
    decoder_buffer_.u.YUVA.u_stride =
        image_planes->RowBytes(SkYUVAIndex::kU_Index);
    decoder_buffer_.u.YUVA.u_size =
        decoder_buffer_.u.YUVA.u_stride *
        DecodedYUVSize(SkYUVAIndex::kU_Index).Height();
    decoder_buffer_.u.YUVA.v_stride =
        image_planes->RowBytes(SkYUVAIndex::kV_Index);
    decoder_buffer_.u.YUVA.v_size =
        decoder_buffer_.u.YUVA.v_stride *
        DecodedYUVSize(SkYUVAIndex::kV_Index).Height();

    decoder_buffer_.is_external_memory = 1;
    decoder_ = WebPINewDecoder(&decoder_buffer_);
    if (!decoder_)
      return SetFailed();
  }

  if (WebPIUpdate(decoder_, data_bytes, data_size) != VP8_STATUS_OK) {
    Clear();
    return SetFailed();
  }

  // TODO(crbug.com/911246): Do post-processing once skcms_Transform
  // supports multiplanar formats.
  ClearDecoder();
  return true;
}

bool WEBPImageDecoder::DecodeSingleFrame(const uint8_t* data_bytes,
                                         size_t data_size,
                                         size_t frame_index) {
  DCHECK(!IsDoingYuvDecode());
  if (Failed())
    return false;
  DCHECK(IsDecodedSizeAvailable());

  DCHECK_GT(frame_buffer_cache_.size(), frame_index);
  ImageFrame& buffer = frame_buffer_cache_[frame_index];
  DCHECK_NE(buffer.GetStatus(), ImageFrame::kFrameComplete);

  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    if (!buffer.AllocatePixelData(Size().Width(), Size().Height(),
                                  ColorSpaceForSkImages())) {
      return SetFailed();
    }
    buffer.ZeroFillPixelData();
    buffer.SetStatus(ImageFrame::kFramePartial);
    // The buffer is transparent outside the decoded area while the image
    // is loading. The correct alpha value for the frame will be set when
    // it is fully decoded.
    buffer.SetHasAlpha(true);
    buffer.SetOriginalFrameRect(IntRect(IntPoint(), Size()));
  }

  const IntRect& frame_rect = buffer.OriginalFrameRect();
  if (!decoder_) {
    // Set up decoder_buffer_ with output mode
    WebPInitDecBuffer(&decoder_buffer_);
    decoder_buffer_.colorspace = RGBOutputMode();
    decoder_buffer_.u.RGBA.stride =
        Size().Width() * sizeof(ImageFrame::PixelData);
    decoder_buffer_.u.RGBA.size =
        decoder_buffer_.u.RGBA.stride * frame_rect.Height();
    decoder_buffer_.is_external_memory = 1;
    decoder_ = WebPINewDecoder(&decoder_buffer_);
    if (!decoder_)
      return SetFailed();
  }
  decoder_buffer_.u.RGBA.rgba = reinterpret_cast<uint8_t*>(
      buffer.GetAddr(frame_rect.X(), frame_rect.Y()));

  switch (WebPIUpdate(decoder_, data_bytes, data_size)) {
    case VP8_STATUS_OK:
      ApplyPostProcessing(frame_index);
      buffer.SetHasAlpha((format_flags_ & ALPHA_FLAG) ||
                         frame_background_has_alpha_);
      buffer.SetStatus(ImageFrame::kFrameComplete);
      ClearDecoder();
      return true;
    case VP8_STATUS_SUSPENDED:
      if (!IsAllDataReceived() && !FrameIsReceivedAtIndex(frame_index)) {
        ApplyPostProcessing(frame_index);
        return false;
      }
      FALLTHROUGH;
    default:
      Clear();
      return SetFailed();
  }
}

cc::ImageHeaderMetadata WEBPImageDecoder::MakeMetadataForDecodeAcceleration()
    const {
  cc::ImageHeaderMetadata image_metadata =
      ImageDecoder::MakeMetadataForDecodeAcceleration();

  DCHECK(consolidated_data_);
  image_metadata.webp_is_non_extended_lossy =
      IsSimpleLossyWebPImage(consolidated_data_);
  return image_metadata;
}

}  // namespace blink
