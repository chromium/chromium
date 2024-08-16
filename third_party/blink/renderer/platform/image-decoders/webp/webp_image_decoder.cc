// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/webp/webp_image_decoder.h"

#include <string.h>

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace {

// Returns two point ranges (<left, width> pairs) at row |canvasY| which belong
// to |src| but not |dst|. A range is empty if its width is 0.
inline void findBlendRangeAtRow(const gfx::Rect& src,
                                const gfx::Rect& dst,
                                int canvasY,
                                int& left1,
                                int& width1,
                                int& left2,
                                int& width2) {
  SECURITY_DCHECK(canvasY >= src.y() && canvasY < src.bottom());
  left1 = -1;
  width1 = 0;
  left2 = -1;
  width2 = 0;

  if (canvasY < dst.y() || canvasY >= dst.bottom() || src.x() >= dst.right() ||
      src.right() <= dst.x()) {
    left1 = src.x();
    width1 = src.width();
    return;
  }

  if (src.x() < dst.x()) {
    left1 = src.x();
    width1 = dst.x() - src.x();
  }

  if (src.right() > dst.right()) {
    left2 = dst.right();
    width2 = src.right() - dst.right();
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
enum class WebPFileFormat {
  kSimpleLossy = 0,
  kSimpleLossless = 1,
  kExtendedAlpha = 2,
  kExtendedAnimation = 3,
  kExtendedAnimationWithAlpha = 4,
  kUnknown = 5,
  kMaxValue = kUnknown,
};

// Validates that |blob| is a simple lossy WebP image. Note that this explicitly
// checks "WEBPVP8 " to exclude extended lossy WebPs that don't actually use any
// extended features.
//
// TODO(crbug.com/1009237): consider combining this with the logic to detect
// WebPs that can be decoded to YUV.
bool IsSimpleLossyWebPImage(const sk_sp<SkData>& blob) {
  if (blob->size() < 20UL) {
    return false;
  }
  DCHECK(blob->bytes());
  return !memcmp(blob->bytes(), "RIFF", 4) &&
         !memcmp(blob->bytes() + 8UL, "WEBPVP8 ", 8);
}

// This method parses |blob|'s header and emits a UMA with the file format, as
// defined by WebP, see WebPFileFormat.
void UpdateWebPFileFormatUMA(const sk_sp<SkData>& blob) {
  if (!IsMainThread()) {
    return;
  }

  WebPBitstreamFeatures features;
  if (WebPGetFeatures(blob->bytes(), blob->size(), &features) !=
      VP8_STATUS_OK) {
    return;
  }

  // These constants are defined verbatim in
  // webp_dec.c::ParseHeadersInternal().
  constexpr int kLossyFormat = 1;
  constexpr int kLosslessFormat = 2;

  WebPFileFormat file_format = WebPFileFormat::kUnknown;
  if (features.has_alpha && features.has_animation) {
    file_format = WebPFileFormat::kExtendedAnimationWithAlpha;
  } else if (features.has_animation) {
    file_format = WebPFileFormat::kExtendedAnimation;
  } else if (features.has_alpha) {
    file_format = WebPFileFormat::kExtendedAlpha;
  } else if (features.format == kLossyFormat) {
    file_format = WebPFileFormat::kSimpleLossy;
  } else if (features.format == kLosslessFormat) {
    file_format = WebPFileFormat::kSimpleLossless;
  }

  UMA_HISTOGRAM_ENUMERATION("Blink.DecodedImage.WebPFileFormat", file_format);
}

}  // namespace

namespace blink {

WEBPImageDecoder::WEBPImageDecoder(AlphaOption alpha_option,
                                   ColorBehavior color_behavior,
                                   wtf_size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   cc::AuxImage::kDefault,
                   max_decoded_bytes) {
  blend_function_ = (alpha_option == kAlphaPremultiplied)
                        ? alphaBlendPremultiplied
                        : alphaBlendNonPremultiplied;
}

WEBPImageDecoder::~WEBPImageDecoder() {
  Clear();
}

String WEBPImageDecoder::FilenameExtension() const {
  return "webp";
}

const AtomicString& WEBPImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, webp_mime_type, ("image/webp"));
  return webp_mime_type;
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

bool WEBPImageDecoder::CanAllowYUVDecodingForWebP() const {
  // Should have been updated with a recent call to UpdateDemuxer().
  if (demux_state_ >= WEBP_DEMUX_PARSED_HEADER &&
      WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT)) {
    // TODO(crbug/910276): Change after alpha support.
    if (!is_lossy_not_animated_no_alpha_) {
      return false;
    }

    // TODO(crbug/911246): Stop vetoing images with ICCP after Skia supports
    // transforming colorspace within YUV, which would allow colorspace
    // conversion during decode. Alternatively, look into passing along
    // transform for raster-time.
    bool has_iccp = !!(format_flags_ & ICCP_FLAG);
    return !has_iccp;
  }
  return false;
}

void WEBPImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  have_parsed_current_data_ = false;
  // TODO(crbug.com/943519): Modify this approach for incremental YUV (when
  // we don't require IsAllDataReceived() to be true before decoding).
  if (IsAllDataReceived()) {
    UpdateDemuxer();
    allow_decode_to_yuv_ = CanAllowYUVDecodingForWebP();
  }
}

int WEBPImageDecoder::RepetitionCount() const {
  return Failed() ? kAnimationLoopOnce : repetition_count_;
}

bool WEBPImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  if (!demux_ || demux_state_ < WEBP_DEMUX_PARSED_HEADER) {
    return false;
  }
  if (!(format_flags_ & ANIMATION_FLAG)) {
    return ImageDecoder::FrameIsReceivedAtIndex(index);
  }
  // frame_buffer_cache_.size() is equal to the return value of
  // DecodeFrameCount(). WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT) returns the
  // number of ANMF chunks that have been received. (See also the DCHECK on
  // animated_frame.complete in InitializeNewFrame().) Therefore we can return
  // true if |index| is valid for frame_buffer_cache_.
  bool frame_is_received_at_index = index < frame_buffer_cache_.size();
  return frame_is_received_at_index;
}

base::TimeDelta WEBPImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Duration()
             : base::TimeDelta();
}

bool WEBPImageDecoder::UpdateDemuxer() {
  if (Failed()) {
    return false;
  }

  // RIFF header (12 bytes) + data chunk header (8 bytes).
  const unsigned kWebpHeaderSize = 20;
  // The number of bytes needed to retrieve the size will vary based on the
  // type of chunk (VP8/VP8L/VP8X). This check just serves as an early out
  // before bitstream validation can occur.
  if (data_->size() < kWebpHeaderSize) {
    return IsAllDataReceived() ? SetFailed() : false;
  }

  if (have_parsed_current_data_) {
    return true;
  }
  have_parsed_current_data_ = true;

  if (consolidated_data_ && consolidated_data_->size() >= data_->size()) {
    // Less data provided than last time. |consolidated_data_| is guaranteed
    // to be its own copy of the data, so it is safe to keep it.
    return true;
  }

  if (IsAllDataReceived() && !consolidated_data_) {
    consolidated_data_ = data_->GetAsSkData();
  } else {
    buffer_.reserve(base::checked_cast<wtf_size_t>(data_->size()));
    while (buffer_.size() < data_->size()) {
      const char* segment;
      const size_t bytes = data_->GetSomeData(segment, buffer_.size());
      DCHECK(bytes);
      buffer_.Append(segment, base::checked_cast<wtf_size_t>(bytes));
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
  const bool truncated_file =
      IsAllDataReceived() && demux_state_ != WEBP_DEMUX_DONE;
  if (!demux_ || demux_state_ < WEBP_DEMUX_PARSED_HEADER || truncated_file) {
    if (!demux_) {
      consolidated_data_.reset();
    } else {
      // We delete the demuxer early to avoid breaking the expectation that
      // frame count == 0 when IsSizeAvailable() is false.
      WebPDemuxDelete(demux_);
      demux_ = nullptr;
    }
    return truncated_file ? SetFailed() : false;
  }

  DCHECK_GE(demux_state_, WEBP_DEMUX_PARSED_HEADER);
  if (!WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT)) {
    return false;  // Wait until the encoded image frame data arrives.
  }

  if (!IsDecodedSizeAvailable()) {
    uint32_t width = WebPDemuxGetI(demux_, WEBP_FF_CANVAS_WIDTH);
    uint32_t height = WebPDemuxGetI(demux_, WEBP_FF_CANVAS_HEIGHT);
    if (!SetSize(base::strict_cast<unsigned>(width),
                 base::strict_cast<unsigned>(height))) {
      return SetFailed();
    }

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

    if ((format_flags_ & ICCP_FLAG) && !IgnoresColorSpace()) {
      ReadColorProfile();
    }

    // Record bpp information only for lossy still images that do not have
    // alpha.
    if (!(format_flags_ & (ANIMATION_FLAG | ALPHA_FLAG))) {
      WebPBitstreamFeatures features;
      CHECK_EQ(WebPGetFeatures(consolidated_data_->bytes(),
                               consolidated_data_->size(), &features),
               VP8_STATUS_OK);
      if (features.format == CompressionFormat::kLossyFormat) {
        is_lossy_not_animated_no_alpha_ = true;
        static constexpr char kType[] = "WebP";
        update_bpp_histogram_callback_ =
            base::BindOnce(&UpdateBppHistogram<kType>);
      }
    }
  }

  DCHECK(IsDecodedSizeAvailable());

  wtf_size_t frame_count = WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT);
  UpdateAggressivePurging(frame_count);

  return true;
}

void WEBPImageDecoder::OnInitFrameBuffer(wtf_size_t frame_index) {
  // ImageDecoder::InitFrameBuffer does a DCHECK if |frame_index| exists.
  ImageFrame& buffer = frame_buffer_cache_[frame_index];

  const wtf_size_t required_previous_frame_index =
      buffer.RequiredPreviousFrameIndex();
  if (required_previous_frame_index == kNotFound) {
    frame_background_has_alpha_ =
        !buffer.OriginalFrameRect().Contains(gfx::Rect(Size()));
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

  // Only 8-bit YUV decode is currently supported.
  DCHECK_EQ(image_planes_->color_type(), kGray_8_SkColorType);

  if (Failed()) {
    return;
  }

  DCHECK(demux_);
  DCHECK(!(format_flags_ & ANIMATION_FLAG));

  WebPIterator webp_iter;
  // libwebp is 1-indexed.
  if (!WebPDemuxGetFrame(demux_, 1 /* frame */, &webp_iter)) {
    SetFailed();
  } else {
    std::unique_ptr<WebPIterator, void (*)(WebPIterator*)> webp_frame(
        &webp_iter, WebPDemuxReleaseIterator);
    DecodeSingleFrameToYUV(
        webp_frame->fragment.bytes,
        base::checked_cast<wtf_size_t>(webp_frame->fragment.size));
  }
}

gfx::Size WEBPImageDecoder::DecodedYUVSize(cc::YUVIndex index) const {
  DCHECK(IsDecodedSizeAvailable());
  switch (index) {
    case cc::YUVIndex::kY:
      return Size();
    case cc::YUVIndex::kU:
    case cc::YUVIndex::kV:
      return gfx::Size((Size().width() + 1) / 2, (Size().height() + 1) / 2);
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::Size(0, 0);
}

wtf_size_t WEBPImageDecoder::DecodedYUVWidthBytes(cc::YUVIndex index) const {
  switch (index) {
    case cc::YUVIndex::kY:
      return base::checked_cast<wtf_size_t>(Size().width());
    case cc::YUVIndex::kU:
    case cc::YUVIndex::kV:
      return base::checked_cast<wtf_size_t>((Size().width() + 1) / 2);
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

SkYUVColorSpace WEBPImageDecoder::GetYUVColorSpace() const {
  return SkYUVColorSpace::kRec601_SkYUVColorSpace;
}

cc::YUVSubsampling WEBPImageDecoder::GetYUVSubsampling() const {
  DCHECK(consolidated_data_);
  if (IsSimpleLossyWebPImage(consolidated_data_)) {
    return cc::YUVSubsampling::k420;
  }
  // It is possible for a non-simple lossy WebP to also be YUV 4:2:0. However,
  // we're being conservative here because this is currently only used for
  // hardware decode acceleration, and WebPs other than simple lossy are not
  // supported in that path anyway.
  return cc::YUVSubsampling::kUnknown;
}

bool WEBPImageDecoder::CanReusePreviousFrameBuffer(
    wtf_size_t frame_index) const {
  DCHECK(frame_index < frame_buffer_cache_.size());
  return frame_buffer_cache_[frame_index].GetAlphaBlendSource() !=
         ImageFrame::kBlendAtopPreviousFrame;
}

void WEBPImageDecoder::ClearFrameBuffer(wtf_size_t frame_index) {
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

  wtf_size_t profile_size =
      base::checked_cast<wtf_size_t>(chunk_iterator.chunk.size);

  if (auto profile = ColorProfile::Create(
          base::span(chunk_iterator.chunk.bytes, profile_size))) {
    if (profile->GetProfile()->data_color_space == skcms_Signature_RGB) {
      SetEmbeddedColorProfile(std::move(profile));
    }
  } else {
    DLOG(ERROR) << "Failed to parse image ICC profile";
  }

  WebPDemuxReleaseChunkIterator(&chunk_iterator);
}

void WEBPImageDecoder::ApplyPostProcessing(wtf_size_t frame_index) {
  ImageFrame& buffer = frame_buffer_cache_[frame_index];
  int width;
  int decoded_height;
  // TODO(crbug.com/911246): Do post-processing once skcms_Transform
  // supports multiplanar formats.
  DCHECK(!IsDoingYuvDecode());

  if (!WebPIDecGetRGB(decoder_, &decoded_height, &width, nullptr, nullptr)) {
    return;  // See also https://bugs.webkit.org/show_bug.cgi?id=74062
  }
  if (decoded_height <= 0) {
    return;
  }

  const gfx::Rect& frame_rect = buffer.OriginalFrameRect();
  SECURITY_DCHECK(width == frame_rect.width());
  SECURITY_DCHECK(decoded_height <= frame_rect.height());
  const int left = frame_rect.x();
  const int top = frame_rect.y();

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
      const gfx::Rect& prev_rect = prev_buffer.OriginalFrameRect();
      // We need to blend a transparent pixel with the starting value (from just
      // after the InitFrame() call). If the pixel belongs to prev_rect, the
      // starting value was fully transparent, so this is a no-op. Otherwise, we
      // need to blend against the pixel from the previous canvas.
      for (int y = decoded_height_; y < decoded_height; ++y) {
        int canvas_y = top + y;
        int left1, width1, left2, width2;
        findBlendRangeAtRow(frame_rect, prev_rect, canvas_y, left1, width1,
                            left2, width2);
        if (width1 > 0) {
          blend_function_(buffer, prev_buffer, canvas_y, left1, width1);
        }
        if (width2 > 0) {
          blend_function_(buffer, prev_buffer, canvas_y, left2, width2);
        }
      }
    }
  }

  decoded_height_ = decoded_height;
  buffer.SetPixelsChanged(true);
}

void WEBPImageDecoder::DecodeSize() {
  UpdateDemuxer();
}

wtf_size_t WEBPImageDecoder::DecodeFrameCount() {
  // If UpdateDemuxer() fails, return the existing number of frames. This way if
  // we get halfway through the image before decoding fails, we won't suddenly
  // start reporting that the image has zero frames.
  return UpdateDemuxer() ? WebPDemuxGetI(demux_, WEBP_FF_FRAME_COUNT)
                         : frame_buffer_cache_.size();
}

void WEBPImageDecoder::InitializeNewFrame(wtf_size_t index) {
  if (!(format_flags_ & ANIMATION_FLAG)) {
    DCHECK(!index);
    return;
  }
  WebPIterator animated_frame;
  if (!WebPDemuxGetFrame(demux_, index + 1, &animated_frame)) {
    SetFailed();
    return;
  }
  DCHECK_EQ(animated_frame.complete, 1);
  ImageFrame* buffer = &frame_buffer_cache_[index];
  gfx::Rect frame_rect(animated_frame.x_offset, animated_frame.y_offset,
                       animated_frame.width, animated_frame.height);
  buffer->SetOriginalFrameRect(IntersectRects(frame_rect, gfx::Rect(Size())));
  buffer->SetDuration(base::Milliseconds(animated_frame.duration));
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

void WEBPImageDecoder::Decode(wtf_size_t index) {
  DCHECK(!IsDoingYuvDecode());

  if (Failed()) {
    return;
  }

  Vector<wtf_size_t> frames_to_decode = FindFramesToDecode(index);

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
      DecodeSingleFrame(
          webp_frame->fragment.bytes,
          base::checked_cast<wtf_size_t>(webp_frame->fragment.size), *i);
    }

    if (Failed()) {
      return;
    }

    // If this returns false, we need more data to continue decoding.
    if (!PostDecodeProcessing(*i)) {
      break;
    }
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= frame_buffer_cache_.size() - 1 && IsAllDataReceived() &&
      demux_ && demux_state_ != WEBP_DEMUX_DONE) {
    SetFailed();
  }
}

bool WEBPImageDecoder::DecodeSingleFrameToYUV(const uint8_t* data_bytes,
                                              wtf_size_t data_size) {
  DCHECK(IsDoingYuvDecode());
  DCHECK(!Failed());

  bool size_available_after_init = IsSizeAvailable();
  DCHECK(size_available_after_init);

  // Set up decoder_buffer_ with output mode
  if (!decoder_) {
    if (!WebPInitDecBuffer(&decoder_buffer_)) {
      return SetFailed();
    }
    decoder_buffer_.colorspace = MODE_YUV;  // TODO(crbug.com/910276): Change
                                            // after alpha YUV support is added.
  }

  ImagePlanes* image_planes = image_planes_.get();
  DCHECK(image_planes);
  // Even if |decoder_| already exists, we must get most up-to-date pointers
  // because memory location might change e.g. upon tab resume.
  decoder_buffer_.u.YUVA.y =
      static_cast<uint8_t*>(image_planes->Plane(cc::YUVIndex::kY));
  decoder_buffer_.u.YUVA.u =
      static_cast<uint8_t*>(image_planes->Plane(cc::YUVIndex::kU));
  decoder_buffer_.u.YUVA.v =
      static_cast<uint8_t*>(image_planes->Plane(cc::YUVIndex::kV));

  if (!decoder_) {
    // libwebp only supports YUV 420 subsampling
    decoder_buffer_.u.YUVA.y_stride = image_planes->RowBytes(cc::YUVIndex::kY);
    decoder_buffer_.u.YUVA.y_size = decoder_buffer_.u.YUVA.y_stride *
                                    DecodedYUVSize(cc::YUVIndex::kY).height();
    decoder_buffer_.u.YUVA.u_stride = image_planes->RowBytes(cc::YUVIndex::kU);
    decoder_buffer_.u.YUVA.u_size = decoder_buffer_.u.YUVA.u_stride *
                                    DecodedYUVSize(cc::YUVIndex::kU).height();
    decoder_buffer_.u.YUVA.v_stride = image_planes->RowBytes(cc::YUVIndex::kV);
    decoder_buffer_.u.YUVA.v_size = decoder_buffer_.u.YUVA.v_stride *
                                    DecodedYUVSize(cc::YUVIndex::kV).height();

    decoder_buffer_.is_external_memory = 1;
    decoder_ = WebPINewDecoder(&decoder_buffer_);
    if (!decoder_) {
      return SetFailed();
    }
  }

  if (WebPIUpdate(decoder_, data_bytes, data_size) != VP8_STATUS_OK) {
    Clear();
    return SetFailed();
  }

  // TODO(crbug.com/911246): Do post-processing once skcms_Transform
  // supports multiplanar formats.
  ClearDecoder();
  image_planes->SetHasCompleteScan();
  if (IsAllDataReceived() && update_bpp_histogram_callback_) {
    std::move(update_bpp_histogram_callback_).Run(Size(), data_->size());
  }
  return true;
}

bool WEBPImageDecoder::DecodeSingleFrame(const uint8_t* data_bytes,
                                         wtf_size_t data_size,
                                         wtf_size_t frame_index) {
  DCHECK(!IsDoingYuvDecode());
  if (Failed()) {
    return false;
  }
  DCHECK(IsDecodedSizeAvailable());

  DCHECK_GT(frame_buffer_cache_.size(), frame_index);
  ImageFrame& buffer = frame_buffer_cache_[frame_index];
  DCHECK_NE(buffer.GetStatus(), ImageFrame::kFrameComplete);

  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    if (!buffer.AllocatePixelData(Size().width(), Size().height(),
                                  ColorSpaceForSkImages())) {
      return SetFailed();
    }
    buffer.ZeroFillPixelData();
    buffer.SetStatus(ImageFrame::kFramePartial);
    // The buffer is transparent outside the decoded area while the image
    // is loading. The correct alpha value for the frame will be set when
    // it is fully decoded.
    buffer.SetHasAlpha(true);
    buffer.SetOriginalFrameRect(gfx::Rect(Size()));
  }

  const gfx::Rect& frame_rect = buffer.OriginalFrameRect();
  if (!decoder_) {
    // Set up decoder_buffer_ with output mode
    if (!WebPInitDecBuffer(&decoder_buffer_)) {
      return SetFailed();
    }
    decoder_buffer_.colorspace = RGBOutputMode();
    decoder_buffer_.u.RGBA.stride =
        Size().width() * sizeof(ImageFrame::PixelData);
    decoder_buffer_.u.RGBA.size =
        decoder_buffer_.u.RGBA.stride * frame_rect.height();
    decoder_buffer_.is_external_memory = 1;
    decoder_ = WebPINewDecoder(&decoder_buffer_);
    if (!decoder_) {
      return SetFailed();
    }
  }
  decoder_buffer_.u.RGBA.rgba = reinterpret_cast<uint8_t*>(
      buffer.GetAddr(frame_rect.x(), frame_rect.y()));

  switch (WebPIUpdate(decoder_, data_bytes, data_size)) {
    case VP8_STATUS_OK:
      ApplyPostProcessing(frame_index);
      buffer.SetHasAlpha((format_flags_ & ALPHA_FLAG) ||
                         frame_background_has_alpha_);
      buffer.SetStatus(ImageFrame::kFrameComplete);
      ClearDecoder();
      if (IsAllDataReceived() && update_bpp_histogram_callback_) {
        std::move(update_bpp_histogram_callback_).Run(Size(), data_->size());
      }
      return true;
    case VP8_STATUS_SUSPENDED:
      if (!IsAllDataReceived() && !FrameIsReceivedAtIndex(frame_index)) {
        ApplyPostProcessing(frame_index);
        return false;
      }
      [[fallthrough]];
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

bool WEBPImageDecoder::FrameStatusSufficientForSuccessors(wtf_size_t index) {
  DCHECK(index < frame_buffer_cache_.size());
  return frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete;
}

}  // namespace blink
