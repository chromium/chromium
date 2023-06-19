// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"

#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "base/bits.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/half_float_maker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/libavif/src/include/avif/avif.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/half_float.h"
#include "ui/gfx/icc_profile.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace {

// The maximum AVIF file size we are willing to decode. This helps libavif
// detect invalid sizes and offsets in an AVIF file before the file size is
// known.
constexpr uint64_t kMaxAvifFileSize = 0x10000000;  // 256 MB

const char* AvifDecoderErrorMessage(const avifDecoder* decoder) {
  // decoder->diag.error is a char array that stores a null-terminated C string.
  return *decoder->diag.error != '\0' ? decoder->diag.error
                                      : "(no error message)";
}

// Builds a gfx::ColorSpace from the ITU-T H.273 (CICP) color description in the
// image. This color space is used to create the gfx::ColorTransform for the
// YUV-to-RGB conversion. If the image does not have an ICC profile, this color
// space is also used to create the embedded color profile.
gfx::ColorSpace GetColorSpace(const avifImage* image) {
  // (As of ISO/IEC 23000-22:2019 Amendment 2) MIAF Section 7.3.6.4 says:
  //   If a coded image has no associated colour property, the default property
  //   is defined as having colour_type equal to 'nclx' with properties as
  //   follows:
  //   – colour_primaries equal to 1,
  //   - transfer_characteristics equal to 13,
  //   - matrix_coefficients equal to 5 or 6 (which are functionally identical),
  //     and
  //   - full_range_flag equal to 1.
  //   ...
  // These values correspond to AVIF_COLOR_PRIMARIES_BT709,
  // AVIF_TRANSFER_CHARACTERISTICS_SRGB, and AVIF_MATRIX_COEFFICIENTS_BT601,
  // respectively.
  //
  // Note that this only specifies the default color property when the color
  // property is absent. It does not really specify the default values for
  // colour_primaries, transfer_characteristics, and matrix_coefficients when
  // they are equal to 2 (unspecified). But we will interpret it as specifying
  // the default values for these variables because we must choose some defaults
  // and these are the most reasonable defaults to choose. We also advocate that
  // all AVIF decoders choose these defaults:
  // https://github.com/AOMediaCodec/av1-avif/issues/84
  const auto primaries =
      image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED
          ? AVIF_COLOR_PRIMARIES_BT709
          : image->colorPrimaries;
  const auto transfer = image->transferCharacteristics ==
                                AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED
                            ? AVIF_TRANSFER_CHARACTERISTICS_SRGB
                            : image->transferCharacteristics;
  const auto matrix =
      image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED
          ? AVIF_MATRIX_COEFFICIENTS_BT601
          : image->matrixCoefficients;
  const auto range = image->yuvRange == AVIF_RANGE_FULL
                         ? gfx::ColorSpace::RangeID::FULL
                         : gfx::ColorSpace::RangeID::LIMITED;
  media::VideoColorSpace color_space(primaries, transfer, matrix, range);
  if (color_space.IsSpecified())
    return color_space.ToGfxColorSpace();
  // media::VideoColorSpace and gfx::ColorSpace do not support CICP
  // MatrixCoefficients 12, 13, 14.
  DCHECK_GE(matrix, 12);
  DCHECK_LE(matrix, 14);
  if (image->yuvRange == AVIF_RANGE_FULL)
    return gfx::ColorSpace::CreateJpeg();
  return gfx::ColorSpace::CreateREC709();
}

// Returns whether media::PaintCanvasVideoRenderer (PCVR) can convert the YUV
// color space of |image| to RGB.
// media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels() uses libyuv
// for the YUV-to-RGB conversion.
bool IsColorSpaceSupportedByPCVR(const avifImage* image) {
  SkYUVColorSpace yuv_color_space;
  // libyuv supports the 8-bit and 10-bit YUVA pixel formats.
  return GetColorSpace(image).ToSkYUVColorSpace(image->depth,
                                                &yuv_color_space) &&
         (!image->alphaPlane ||
          ((image->depth == 8 || image->depth == 10) &&
           (image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420 ||
            image->yuvFormat == AVIF_PIXEL_FORMAT_YUV422 ||
            image->yuvFormat == AVIF_PIXEL_FORMAT_YUV444)));
}

media::VideoPixelFormat AvifToVideoPixelFormat(avifPixelFormat fmt,
                                               bool has_alpha,
                                               int depth) {
  if (depth != 8 && depth != 10 && depth != 12) {
    // Unsupported bit depth.
    NOTREACHED();
    return media::PIXEL_FORMAT_UNKNOWN;
  }
  int depth_index = (depth - 8) / 2;
  // In these lookup tables, the first index is has_alpha and the second index
  // is depth_index. Note that there are no media::VideoPixelFormat values for
  // 12-bit YUVA.
  static constexpr media::VideoPixelFormat kYUV420Formats[][3] = {
      {media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_YUV420P10,
       media::PIXEL_FORMAT_YUV420P12},
      {media::PIXEL_FORMAT_I420A, media::PIXEL_FORMAT_YUV420AP10,
       media::PIXEL_FORMAT_UNKNOWN}};
  static constexpr media::VideoPixelFormat kYUV422Formats[][3] = {
      {media::PIXEL_FORMAT_I422, media::PIXEL_FORMAT_YUV422P10,
       media::PIXEL_FORMAT_YUV422P12},
      {media::PIXEL_FORMAT_I422A, media::PIXEL_FORMAT_YUV422AP10,
       media::PIXEL_FORMAT_UNKNOWN}};
  static constexpr media::VideoPixelFormat kYUV444Formats[][3] = {
      {media::PIXEL_FORMAT_I444, media::PIXEL_FORMAT_YUV444P10,
       media::PIXEL_FORMAT_YUV444P12},
      {media::PIXEL_FORMAT_I444A, media::PIXEL_FORMAT_YUV444AP10,
       media::PIXEL_FORMAT_UNKNOWN}};
  switch (fmt) {
    case AVIF_PIXEL_FORMAT_YUV420:
    case AVIF_PIXEL_FORMAT_YUV400:
      return kYUV420Formats[has_alpha][depth_index];
    case AVIF_PIXEL_FORMAT_YUV422:
      return kYUV422Formats[has_alpha][depth_index];
    case AVIF_PIXEL_FORMAT_YUV444:
      return kYUV444Formats[has_alpha][depth_index];
    case AVIF_PIXEL_FORMAT_NONE:
    case AVIF_PIXEL_FORMAT_COUNT:
      NOTREACHED();
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

// |y_size| is the width or height of the Y plane. Returns the width or height
// of the U and V planes. |chroma_shift| represents the subsampling of the
// chroma (U and V) planes in the x (for width) or y (for height) direction.
int UVSize(int y_size, int chroma_shift) {
  DCHECK(chroma_shift == 0 || chroma_shift == 1);
  return (y_size + chroma_shift) >> chroma_shift;
}

inline void WritePixel(float max_channel,
                       const gfx::Point3F& pixel,
                       float alpha,
                       bool premultiply_alpha,
                       uint32_t* rgba_dest) {
  uint8_t r = base::ClampRound<uint8_t>(pixel.x() * 255.0f);
  uint8_t g = base::ClampRound<uint8_t>(pixel.y() * 255.0f);
  uint8_t b = base::ClampRound<uint8_t>(pixel.z() * 255.0f);
  uint8_t a = base::ClampRound<uint8_t>(alpha * 255.0f);
  if (premultiply_alpha)
    blink::ImageFrame::SetRGBAPremultiply(rgba_dest, r, g, b, a);
  else
    *rgba_dest = SkPackARGB32NoCheck(a, r, g, b);
}

inline void WritePixel(float max_channel,
                       const gfx::Point3F& pixel,
                       float alpha,
                       bool premultiply_alpha,
                       uint64_t* rgba_dest) {
  float rgba_pixels[4];
  rgba_pixels[0] = pixel.x();
  rgba_pixels[1] = pixel.y();
  rgba_pixels[2] = pixel.z();
  rgba_pixels[3] = alpha;
  if (premultiply_alpha && alpha != 1.0f) {
    rgba_pixels[0] *= alpha;
    rgba_pixels[1] *= alpha;
    rgba_pixels[2] *= alpha;
  }

  gfx::FloatToHalfFloat(rgba_pixels, reinterpret_cast<uint16_t*>(rgba_dest),
                        std::size(rgba_pixels));
}

enum class ColorType { kMono, kColor };

template <ColorType color_type, typename InputType, typename OutputType>
void YUVAToRGBA(const avifImage* image,
                const gfx::ColorTransform* transform,
                bool premultiply_alpha,
                OutputType* rgba_dest) {
  avifPixelFormatInfo format_info;
  avifGetPixelFormatInfo(image->yuvFormat, &format_info);
  gfx::Point3F pixel;
  const int max_channel_i = (1 << image->depth) - 1;
  const float max_channel = static_cast<float>(max_channel_i);
  for (uint32_t j = 0; j < image->height; ++j) {
    const int uv_j = j >> format_info.chromaShiftY;

    const InputType* y_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_Y][j * image->yuvRowBytes[AVIF_CHAN_Y]]);
    const InputType* u_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_U][uv_j * image->yuvRowBytes[AVIF_CHAN_U]]);
    const InputType* v_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_V][uv_j * image->yuvRowBytes[AVIF_CHAN_V]]);
    const InputType* a_ptr = nullptr;
    if (image->alphaPlane) {
      a_ptr = reinterpret_cast<InputType*>(
          &image->alphaPlane[j * image->alphaRowBytes]);
    }

    for (uint32_t i = 0; i < image->width; ++i) {
      const int uv_i = i >> format_info.chromaShiftX;
      pixel.set_x(y_ptr[i] / max_channel);
      if (color_type == ColorType::kColor) {
        pixel.set_y(u_ptr[uv_i] / max_channel);
        pixel.set_z(v_ptr[uv_i] / max_channel);
      } else {
        pixel.set_y(0.5f);
        pixel.set_z(0.5f);
      }

      transform->Transform(&pixel, 1);

      // TODO(wtc): Use templates or other ways to avoid checking whether the
      // image supports alpha in the inner loop.
      const int alpha = a_ptr ? a_ptr[i] : max_channel_i;

      WritePixel(max_channel, pixel, alpha / max_channel, premultiply_alpha,
                 rgba_dest);
      rgba_dest++;
    }
  }
}

}  // namespace

namespace blink {

AVIFImageDecoder::AVIFImageDecoder(AlphaOption alpha_option,
                                   HighBitDepthDecodingOption hbd_option,
                                   const ColorBehavior& color_behavior,
                                   wtf_size_t max_decoded_bytes,
                                   AnimationOption animation_option)
    : ImageDecoder(alpha_option, hbd_option, color_behavior, max_decoded_bytes),
      animation_option_(animation_option) {}

AVIFImageDecoder::~AVIFImageDecoder() = default;

const AtomicString& AVIFImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, avif_mime_type, ("image/avif"));
  return avif_mime_type;
}

bool AVIFImageDecoder::ImageIsHighBitDepth() {
  return bit_depth_ > 8;
}

void AVIFImageDecoder::OnSetData(SegmentReader* data) {
  have_parsed_current_data_ = false;
  const bool all_data_received = IsAllDataReceived();
  avif_io_data_.reader = data_.get();
  avif_io_data_.all_data_received = all_data_received;
  avif_io_.sizeHint = all_data_received ? data_->size() : kMaxAvifFileSize;

  // ImageFrameGenerator::GetYUVAInfo() and ImageFrameGenerator::DecodeToYUV()
  // assume that allow_decode_to_yuv_ and other image metadata are available
  // after calling ImageDecoder::Create() with data_complete=true.
  if (all_data_received)
    ParseMetadata();
}

cc::YUVSubsampling AVIFImageDecoder::GetYUVSubsampling() const {
  switch (avif_yuv_format_) {
    case AVIF_PIXEL_FORMAT_YUV420:
      return cc::YUVSubsampling::k420;
    case AVIF_PIXEL_FORMAT_YUV422:
      return cc::YUVSubsampling::k422;
    case AVIF_PIXEL_FORMAT_YUV444:
      return cc::YUVSubsampling::k444;
    case AVIF_PIXEL_FORMAT_YUV400:
      return cc::YUVSubsampling::kUnknown;
    case AVIF_PIXEL_FORMAT_NONE:
      // avif_yuv_format_ is initialized to AVIF_PIXEL_FORMAT_NONE in the
      // constructor. If we have called SetSize() successfully at the end
      // of UpdateDemuxer(), avif_yuv_format_ cannot possibly be
      // AVIF_PIXEL_FORMAT_NONE.
      CHECK(!IsDecodedSizeAvailable());
      return cc::YUVSubsampling::kUnknown;
    case AVIF_PIXEL_FORMAT_COUNT:
      break;
  }
  NOTREACHED_NORETURN() << "Invalid YUV format: " << avif_yuv_format_;
}

gfx::Size AVIFImageDecoder::DecodedYUVSize(cc::YUVIndex index) const {
  DCHECK(IsDecodedSizeAvailable());
  if (index == cc::YUVIndex::kU || index == cc::YUVIndex::kV) {
    return gfx::Size(UVSize(Size().width(), chroma_shift_x_),
                     UVSize(Size().height(), chroma_shift_y_));
  }
  return Size();
}

wtf_size_t AVIFImageDecoder::DecodedYUVWidthBytes(cc::YUVIndex index) const {
  DCHECK(IsDecodedSizeAvailable());
  // Try to return the same width bytes as used by the dav1d library. This will
  // allow DecodeToYUV() to copy each plane with a single memcpy() call.
  //
  // The comments for Dav1dPicAllocator in dav1d/picture.h require the pixel
  // width be padded to a multiple of 128 pixels.
  wtf_size_t aligned_width =
      static_cast<wtf_size_t>(base::bits::AlignUp(Size().width(), 128));
  if (index == cc::YUVIndex::kU || index == cc::YUVIndex::kV) {
    aligned_width >>= chroma_shift_x_;
  }
  // When the stride is a multiple of 1024, dav1d_default_picture_alloc()
  // slightly pads the stride to avoid a reduction in cache hit rate in most
  // L1/L2 cache implementations. Match that trick here. (Note that this padding
  // is not documented in dav1d/picture.h.)
  if ((aligned_width & 1023) == 0)
    aligned_width += 64;

  // High bit depth YUV is stored as a uint16_t, double the number of bytes.
  if (bit_depth_ > 8) {
    DCHECK_LE(bit_depth_, 16);
    aligned_width *= 2;
  }

  return aligned_width;
}

SkYUVColorSpace AVIFImageDecoder::GetYUVColorSpace() const {
  DCHECK(CanDecodeToYUV());
  DCHECK_NE(yuv_color_space_, SkYUVColorSpace::kIdentity_SkYUVColorSpace);
  return yuv_color_space_;
}

uint8_t AVIFImageDecoder::GetYUVBitDepth() const {
  DCHECK(CanDecodeToYUV());
  return bit_depth_;
}

absl::optional<gfx::HDRMetadata> AVIFImageDecoder::GetHDRMetadata() const {
  return hdr_metadata_;
}

void AVIFImageDecoder::DecodeToYUV() {
  DCHECK(image_planes_);
  DCHECK(CanDecodeToYUV());

  if (Failed())
    return;

  DCHECK(decoder_);
  DCHECK_EQ(decoded_frame_count_, 1u);  // Not animation.

  // If the image is decoded progressively, just render the highest progressive
  // frame in image_planes_ because the callers of DecodeToYUV() assume that a
  // complete scan will not be updated.
  const int frame_index = progressive_ ? (decoder_->imageCount - 1) : 0;
  // TODO(crbug.com/943519): Implement YUV incremental decoding as in Decode().
  decoder_->allowIncremental = AVIF_FALSE;

  // libavif cannot decode to an external buffer. So we need to copy from
  // libavif's internal buffer to |image_planes_|.
  // TODO(crbug.com/1099825): Enhance libavif to decode to an external buffer.
  auto ret = DecodeImage(frame_index);
  if (ret != AVIF_RESULT_OK) {
    if (ret != AVIF_RESULT_WAITING_ON_IO)
      SetFailed();
    return;
  }
  const auto* image = decoded_image_;

  DCHECK(!image->alphaPlane);
  static_assert(cc::YUVIndex::kY == static_cast<cc::YUVIndex>(AVIF_CHAN_Y), "");
  static_assert(cc::YUVIndex::kU == static_cast<cc::YUVIndex>(AVIF_CHAN_U), "");
  static_assert(cc::YUVIndex::kV == static_cast<cc::YUVIndex>(AVIF_CHAN_V), "");

  // Disable subnormal floats which can occur when converting to half float.
  std::unique_ptr<cc::ScopedSubnormalFloatDisabler> disable_subnormals;
  const bool is_f16 = image_planes_->color_type() == kA16_float_SkColorType;
  if (is_f16)
    disable_subnormals = std::make_unique<cc::ScopedSubnormalFloatDisabler>();
  const float kHighBitDepthMultiplier =
      (is_f16 ? 1.0f : 65535.0f) / ((1 << bit_depth_) - 1);

  // Initialize |width| and |height| to the width and height of the luma plane.
  uint32_t width = image->width;
  uint32_t height = image->height;

  for (wtf_size_t plane_index = 0; plane_index < cc::kNumYUVPlanes;
       ++plane_index) {
    const cc::YUVIndex plane = static_cast<cc::YUVIndex>(plane_index);
    const wtf_size_t src_row_bytes =
        base::strict_cast<wtf_size_t>(image->yuvRowBytes[plane_index]);
    const wtf_size_t dst_row_bytes = image_planes_->RowBytes(plane);

    if (bit_depth_ == 8) {
      DCHECK_EQ(image_planes_->color_type(), kGray_8_SkColorType);
      const uint8_t* src = image->yuvPlanes[plane_index];
      uint8_t* dst = static_cast<uint8_t*>(image_planes_->Plane(plane));
      libyuv::CopyPlane(src, src_row_bytes, dst, dst_row_bytes, width, height);
    } else {
      DCHECK_GT(bit_depth_, 8u);
      DCHECK_LE(bit_depth_, 16u);
      const uint16_t* src =
          reinterpret_cast<uint16_t*>(image->yuvPlanes[plane_index]);
      uint16_t* dst = static_cast<uint16_t*>(image_planes_->Plane(plane));
      if (image_planes_->color_type() == kA16_unorm_SkColorType) {
        const wtf_size_t src_stride = src_row_bytes / 2;
        const wtf_size_t dst_stride = dst_row_bytes / 2;
        for (uint32_t j = 0; j < height; ++j) {
          for (uint32_t i = 0; i < width; ++i) {
            dst[j * dst_stride + i] =
                src[j * src_stride + i] * kHighBitDepthMultiplier + 0.5f;
          }
        }
      } else if (image_planes_->color_type() == kA16_float_SkColorType) {
        // Note: Unlike CopyPlane_16, HalfFloatPlane wants the stride in bytes.
        libyuv::HalfFloatPlane(src, src_row_bytes, dst, dst_row_bytes,
                               kHighBitDepthMultiplier, width, height);
      } else {
        NOTREACHED() << "Unsupported color type: "
                     << static_cast<int>(image_planes_->color_type());
      }
    }
    if (plane == cc::YUVIndex::kY) {
      // Having processed the luma plane, change |width| and |height| to the
      // width and height of the chroma planes.
      width = UVSize(width, chroma_shift_x_);
      height = UVSize(height, chroma_shift_y_);
    }
  }
  image_planes_->SetHasCompleteScan();
}

int AVIFImageDecoder::RepetitionCount() const {
  if (decoded_frame_count_ > 1) {
    switch (decoder_->repetitionCount) {
      case AVIF_REPETITION_COUNT_INFINITE:
        return kAnimationLoopInfinite;
      case AVIF_REPETITION_COUNT_UNKNOWN:
        // The AVIF file does not have repetitions specified using an EditList
        // box. Loop infinitely for backward compatibility with older versions
        // of Chrome.
        return kAnimationLoopInfinite;
      default:
        return decoder_->repetitionCount;
    }
  }
  return kAnimationNone;
}

bool AVIFImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  if (!IsDecodedSizeAvailable())
    return false;
  if (decoded_frame_count_ == 1)
    return ImageDecoder::FrameIsReceivedAtIndex(index);
  if (index >= frame_buffer_cache_.size())
    return false;
  if (IsAllDataReceived())
    return true;
  avifExtent data_extent;
  if (avifDecoderNthImageMaxExtent(decoder_.get(), index, &data_extent) !=
      AVIF_RESULT_OK) {
    return false;
  }
  return data_extent.size == 0 ||
         data_extent.offset + data_extent.size <= data_->size();
}

absl::optional<base::TimeDelta> AVIFImageDecoder::FrameTimestampAtIndex(
    wtf_size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Timestamp()
             : absl::nullopt;
}

base::TimeDelta AVIFImageDecoder::FrameDurationAtIndex(wtf_size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Duration()
             : base::TimeDelta();
}

bool AVIFImageDecoder::ImageHasBothStillAndAnimatedSubImages() const {
  // Per MIAF, all animated AVIF files must have a still image, even if it's
  // just a pointer to the first frame of the animation.
  if (decoded_frame_count_ > 1)
    return true;

  constexpr wtf_size_t kMajorBrandOffset = 8;
  constexpr wtf_size_t kMajorBrandSize = 4;
  if (data_->size() < kMajorBrandOffset + kMajorBrandSize)
    return false;

  // TODO(wtc): We should rely on libavif to tell us if the file has both an
  // image and an animation track instead of just checking the major brand.
  //
  // An AVIF image begins with a file‐type box 'ftyp':
  //   unsigned int(32) size;
  //   unsigned int(32) type = boxtype;  // boxtype is 'ftyp'
  //   unsigned int(32) major_brand;
  //   ...
  FastSharedBufferReader fast_reader(data_);
  char buf[kMajorBrandSize];
  const char* major_brand =
      fast_reader.GetConsecutiveData(kMajorBrandOffset, kMajorBrandSize, buf);
  // The brand 'avis' is an AVIF image sequence (animation) brand.
  return memcmp(major_brand, "avis", kMajorBrandSize) == 0;
}

// static
bool AVIFImageDecoder::MatchesAVIFSignature(
    const FastSharedBufferReader& fast_reader) {
  // avifPeekCompatibleFileType() clamps compatible brands at 32 when reading in
  // the ftyp box in ISO BMFF for the 'avif' or 'avis' brand. So the maximum
  // number of bytes read is 144 bytes (size 4 bytes, type 4 bytes, major brand
  // 4 bytes, minor version 4 bytes, and 4 bytes * 32 compatible brands).
  char buffer[144];
  avifROData input;
  input.size = std::min(sizeof(buffer), fast_reader.size());
  input.data = reinterpret_cast<const uint8_t*>(
      fast_reader.GetConsecutiveData(0, input.size, buffer));
  return avifPeekCompatibleFileType(&input);
}

gfx::ColorTransform* AVIFImageDecoder::GetColorTransformForTesting() {
  UpdateColorTransform(GetColorSpace(decoder_->image), decoder_->image->depth);
  return color_transform_.get();
}

void AVIFImageDecoder::ParseMetadata() {
  if (!UpdateDemuxer())
    SetFailed();
}

void AVIFImageDecoder::DecodeSize() {
  ParseMetadata();
}

wtf_size_t AVIFImageDecoder::DecodeFrameCount() {
  if (!Failed())
    ParseMetadata();
  return IsDecodedSizeAvailable() ? decoded_frame_count_
                                  : frame_buffer_cache_.size();
}

void AVIFImageDecoder::InitializeNewFrame(wtf_size_t index) {
  auto& buffer = frame_buffer_cache_[index];
  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);

  // For AVIFs, the frame always fills the entire image.
  buffer.SetOriginalFrameRect(gfx::Rect(Size()));

  avifImageTiming timing;
  auto ret = avifDecoderNthImageTiming(decoder_.get(), index, &timing);
  DCHECK_EQ(ret, AVIF_RESULT_OK);
  buffer.SetTimestamp(base::Seconds(timing.pts));
  buffer.SetDuration(base::Seconds(timing.duration));
}

void AVIFImageDecoder::Decode(wtf_size_t index) {
  if (Failed())
    return;

  UpdateAggressivePurging(index);

  int frame_index = index;
  // If the image is decoded progressively, find the highest progressive
  // frame that we have received and decode from that frame index. Internally
  // decoder_ still decodes the lower progressive frames, but they are only used
  // as reference frames and not rendered.
  if (progressive_) {
    DCHECK_EQ(index, 0u);
    // decoder_->imageIndex is the current image index. decoder_->imageIndex is
    // initialized to -1. decoder_->imageIndex + 1 is the next image index.
    DCHECK_LT(decoder_->imageIndex + 1, decoder_->imageCount);
    for (frame_index = decoder_->imageIndex + 1;
         frame_index + 1 < decoder_->imageCount; ++frame_index) {
      avifExtent data_extent;
      auto rv = avifDecoderNthImageMaxExtent(decoder_.get(), frame_index + 1,
                                             &data_extent);
      if (rv != AVIF_RESULT_OK) {
        DVLOG(1) << "avifDecoderNthImageMaxExtent(" << frame_index + 1
                 << ") failed: " << avifResultToString(rv) << ": "
                 << AvifDecoderErrorMessage(decoder_.get());
        SetFailed();
        return;
      }
      if (data_extent.size != 0 &&
          data_extent.offset + data_extent.size > data_->size()) {
        break;
      }
    }
  }

  // Allow AVIF frames to be partially decoded before all data is received.
  // Only enabled for non-progressive still images because animations look
  // better without incremental decoding and because progressive decoding makes
  // incremental decoding unnecessary.
  decoder_->allowIncremental = (decoder_->imageCount == 1);

  auto ret = DecodeImage(frame_index);
  if (ret != AVIF_RESULT_OK && ret != AVIF_RESULT_WAITING_ON_IO) {
    SetFailed();
    return;
  }
  const auto* image = decoded_image_;

  // ImageDecoder::SizeCalculationMayOverflow(), called by UpdateDemuxer()
  // before being here, made sure the image height fits in an int.
  int displayable_height =
      static_cast<int>(avifDecoderDecodedRowCount(decoder_.get()));
  if (image == cropped_image_.get()) {
    displayable_height -= clap_origin_.y();
    displayable_height =
        std::clamp(displayable_height, 0, static_cast<int>(image->height));
  }

  if (displayable_height == 0) {
    return;  // There is nothing to display.
  }

  ImageFrame& buffer = frame_buffer_cache_[index];
  DCHECK_NE(buffer.GetStatus(), ImageFrame::kFrameComplete);

  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    if (!InitFrameBuffer(index)) {
      DVLOG(1) << "Failed to create frame buffer...";
      SetFailed();
      return;
    }
    DCHECK_EQ(buffer.GetStatus(), ImageFrame::kFramePartial);
    // The buffer is transparent outside the decoded area while the image is
    // loading. The correct alpha value for the frame will be set when it is
    // fully decoded.
    buffer.SetHasAlpha(true);
    if (decoder_->allowIncremental) {
      // In case of buffer disposal after decoding.
      incrementally_displayed_height_ = 0;
    }
  }

  const int last_displayed_height =
      decoder_->allowIncremental ? incrementally_displayed_height_ : 0;
  if (displayable_height == last_displayed_height) {
    return;  // There is no new row to display.
  }
  DCHECK_GT(displayable_height, last_displayed_height);

  // Only render the newly decoded rows.
  if (!RenderImage(image, last_displayed_height, &displayable_height,
                   &buffer)) {
    SetFailed();
    return;
  }
  if (displayable_height == last_displayed_height) {
    return;  // There is no new row to display.
  }
  DCHECK_GT(displayable_height, last_displayed_height);
  ColorCorrectImage(last_displayed_height, displayable_height, &buffer);
  buffer.SetPixelsChanged(true);
  if (decoder_->allowIncremental) {
    incrementally_displayed_height_ = displayable_height;
  }

  if (static_cast<uint32_t>(displayable_height) == image->height &&
      (!progressive_ || frame_index + 1 == decoder_->imageCount)) {
    buffer.SetHasAlpha(!!image->alphaPlane);
    buffer.SetStatus(ImageFrame::kFrameComplete);
    PostDecodeProcessing(index);
  }
}

bool AVIFImageDecoder::CanReusePreviousFrameBuffer(wtf_size_t index) const {
  // (a) Technically we can reuse the bitmap of the previous frame because the
  // AVIF decoder handles frame dependence internally and we never need to
  // preserve previous frames to decode later ones, and (b) since this function
  // will not currently be called, this is really more for the reader than any
  // functional purpose.
  return true;
}

// static
avifResult AVIFImageDecoder::ReadFromSegmentReader(avifIO* io,
                                                   uint32_t read_flags,
                                                   uint64_t offset,
                                                   size_t size,
                                                   avifROData* out) {
  if (read_flags != 0) {
    // Unsupported read_flags
    return AVIF_RESULT_IO_ERROR;
  }

  AvifIOData* io_data = static_cast<AvifIOData*>(io->data);

  // Sanitize/clamp incoming request
  if (offset > io_data->reader->size()) {
    // The offset is past the end of the buffer or available data.
    return io_data->all_data_received ? AVIF_RESULT_IO_ERROR
                                      : AVIF_RESULT_WAITING_ON_IO;
  }

  // It is more convenient to work with a variable of the wtf_size_t type. Since
  // offset <= io_data->reader->size() <= SIZE_MAX, this cast is safe.
  size_t position = static_cast<size_t>(offset);
  const size_t available_size = io_data->reader->size() - position;
  if (size > available_size) {
    if (!io_data->all_data_received)
      return AVIF_RESULT_WAITING_ON_IO;
    size = available_size;
  }

  out->size = size;
  const char* data;
  size_t data_size = io_data->reader->GetSomeData(data, position);
  if (data_size >= size) {
    out->data = reinterpret_cast<const uint8_t*>(data);
    return AVIF_RESULT_OK;
  }

  io_data->buffer.clear();
  io_data->buffer.reserve(size);
  while (size != 0) {
    data_size = io_data->reader->GetSomeData(data, position);
    size_t copy_size = std::min(data_size, size);
    io_data->buffer.insert(io_data->buffer.end(), data, data + copy_size);
    position += copy_size;
    size -= copy_size;
  }

  out->data = io_data->buffer.data();
  return AVIF_RESULT_OK;
}

bool AVIFImageDecoder::UpdateDemuxer() {
  DCHECK(!Failed());
  if (IsDecodedSizeAvailable())
    return true;

  if (have_parsed_current_data_)
    return true;
  have_parsed_current_data_ = true;

  if (!decoder_) {
    decoder_.reset(avifDecoderCreate());
    if (!decoder_)
      return false;

    // For simplicity, use a hardcoded maxThreads of 2, independent of the image
    // size and processor count. Note: even if we want maxThreads to depend on
    // the image size, it is impossible to do so because maxThreads is passed to
    // dav1d_open() inside avifDecoderParse(), but the image size is not known
    // until avifDecoderParse() returns successfully. See
    // https://github.com/AOMediaCodec/libavif/issues/636.
    decoder_->maxThreads = 2;

    if (animation_option_ != AnimationOption::kUnspecified &&
        avifDecoderSetSource(
            decoder_.get(),
            animation_option_ == AnimationOption::kPreferAnimation
                ? AVIF_DECODER_SOURCE_TRACKS
                : AVIF_DECODER_SOURCE_PRIMARY_ITEM) != AVIF_RESULT_OK) {
      return false;
    }

    // Chrome doesn't use XMP and Exif metadata. Ignoring XMP and Exif will
    // ensure avifDecoderParse() isn't waiting for some tiny Exif payload hiding
    // at the end of a file.
    decoder_->ignoreXMP = AVIF_TRUE;
    decoder_->ignoreExif = AVIF_TRUE;

    // Turn off libavif's 'clap' (clean aperture) property validation. We
    // validate 'clap' ourselves and ignore invalid 'clap' properties.
    decoder_->strictFlags &= ~AVIF_STRICT_CLAP_VALID;
    // Allow the PixelInformationProperty ('pixi') to be missing in AV1 image
    // items. libheif v1.11.0 or older does not add the 'pixi' item property to
    // AV1 image items. (This issue has been corrected in libheif v1.12.0.) See
    // crbug.com/1198455.
    decoder_->strictFlags &= ~AVIF_STRICT_PIXI_REQUIRED;

    avif_io_.destroy = nullptr;
    avif_io_.read = ReadFromSegmentReader;
    avif_io_.write = nullptr;
    avif_io_.persistent = AVIF_FALSE;
    avif_io_.data = &avif_io_data_;
    avifDecoderSetIO(decoder_.get(), &avif_io_);
  }

  // If all data is received, there is no point in decoding progressively.
  decoder_->allowProgressive = !IsAllDataReceived();

  auto ret = avifDecoderParse(decoder_.get());
  if (ret == AVIF_RESULT_WAITING_ON_IO)
    return true;
  if (ret != AVIF_RESULT_OK) {
    DVLOG(1) << "avifDecoderParse failed: " << avifResultToString(ret);
    return false;
  }

  // Image metadata is available in decoder_->image after avifDecoderParse()
  // even though decoder_->imageIndex is invalid (-1).
  DCHECK_EQ(decoder_->imageIndex, -1);
  // This variable is named |container| to emphasize the fact that the current
  // contents of decoder_->image come from the container, not any frame.
  const auto* container = decoder_->image;

  // The container width and container height are read from either the tkhd
  // (track header) box of a track or the ispe (image spatial extents) property
  // of an image item, both of which are mandatory in the spec.
  if (container->width == 0 || container->height == 0) {
    DVLOG(1) << "Container width and height must be present";
    return false;
  }

  // The container depth is read from either the av1C box of a track or the av1C
  // property of an image item, both of which are mandatory in the spec.
  if (container->depth == 0) {
    DVLOG(1) << "Container depth must be present";
    return false;
  }

  DCHECK_GT(decoder_->imageCount, 0);
  progressive_ = decoder_->progressiveState == AVIF_PROGRESSIVE_STATE_ACTIVE;
  // If the image is progressive, decoder_->imageCount is the number of
  // progressive frames, but there is only one still image.
  decoded_frame_count_ = progressive_ ? 1 : decoder_->imageCount;
  container_width_ = container->width;
  container_height_ = container->height;
  bit_depth_ = container->depth;
  decode_to_half_float_ =
      ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat;

  // Verify that AVIF_PIXEL_FORMAT_{YUV444,YUV422,YUV420,YUV400} are
  // consecutive.
  static_assert(AVIF_PIXEL_FORMAT_YUV422 == AVIF_PIXEL_FORMAT_YUV444 + 1);
  static_assert(AVIF_PIXEL_FORMAT_YUV420 == AVIF_PIXEL_FORMAT_YUV422 + 1);
  static_assert(AVIF_PIXEL_FORMAT_YUV400 == AVIF_PIXEL_FORMAT_YUV420 + 1);
  // Assert that after avifDecoderParse() returns AVIF_RESULT_OK,
  // decoder_->image->yuvFormat (the same as container->yuvFormat) is one of the
  // four YUV formats in AV1.
  CHECK(container->yuvFormat >= AVIF_PIXEL_FORMAT_YUV444 &&
        container->yuvFormat <= AVIF_PIXEL_FORMAT_YUV400)
      << "Invalid YUV format: " << container->yuvFormat;
  avif_yuv_format_ = container->yuvFormat;
  avifPixelFormatInfo format_info;
  avifGetPixelFormatInfo(container->yuvFormat, &format_info);
  chroma_shift_x_ = format_info.chromaShiftX;
  chroma_shift_y_ = format_info.chromaShiftY;

  if (container->clli.maxCLL || container->clli.maxPALL) {
    hdr_metadata_ = gfx::HDRMetadata();
    hdr_metadata_->cta_861_3 = gfx::HdrMetadataCta861_3(
        container->clli.maxCLL, container->clli.maxPALL);
  }

  // SetEmbeddedColorProfile() must be called before IsSizeAvailable() becomes
  // true. So call SetEmbeddedColorProfile() before calling SetSize(). The color
  // profile is either an ICC profile or the CICP color description.

  if (!IgnoresColorSpace()) {
    // The CICP color description is always present because we can always get it
    // from the AV1 sequence header for the frames. If an ICC profile is
    // present, use it instead of the CICP color description.
    if (container->icc.size) {
      std::unique_ptr<ColorProfile> profile =
          ColorProfile::Create(container->icc.data, container->icc.size);
      if (!profile) {
        DVLOG(1) << "Failed to parse image ICC profile";
        return false;
      }
      uint32_t data_color_space = profile->GetProfile()->data_color_space;
      const bool is_mono = container->yuvFormat == AVIF_PIXEL_FORMAT_YUV400;
      if (is_mono) {
        if (data_color_space != skcms_Signature_Gray &&
            data_color_space != skcms_Signature_RGB)
          profile = nullptr;
      } else {
        if (data_color_space != skcms_Signature_RGB)
          profile = nullptr;
      }
      if (!profile) {
        DVLOG(1)
            << "Image contains ICC profile that does not match its color space";
        return false;
      }
      SetEmbeddedColorProfile(std::move(profile));
    } else if (container->colorPrimaries != AVIF_COLOR_PRIMARIES_UNSPECIFIED ||
               container->transferCharacteristics !=
                   AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED) {
      gfx::ColorSpace frame_cs = GetColorSpace(container);

      sk_sp<SkColorSpace> sk_color_space =
          frame_cs.GetAsFullRangeRGB().ToSkColorSpace();
      if (!sk_color_space) {
        DVLOG(1) << "Image contains an unsupported color space";
        return false;
      }

      skcms_ICCProfile profile;
      sk_color_space->toProfile(&profile);
      SetEmbeddedColorProfile(std::make_unique<ColorProfile>(profile));
    }
  }

  // |angle| * 90 specifies the angle of anti-clockwise rotation in degrees.
  // Legal values: [0-3].
  int angle = 0;
  if (container->transformFlags & AVIF_TRANSFORM_IROT) {
    angle = container->irot.angle;
    CHECK_LT(angle, 4);
  }
  // |mode| specifies how the mirroring is performed.
  //   -1: No mirroring.
  //    0: The top and bottom parts of the image are exchanged.
  //    1: The left and right parts of the image are exchanged.
  int mode = -1;
  if (container->transformFlags & AVIF_TRANSFORM_IMIR) {
    mode = container->imir.mode;
    CHECK_LT(mode, 2);
  }
  // MIAF Section 7.3.6.7 (Clean aperture, rotation and mirror) says:
  //   These properties, if used, shall be indicated to be applied in the
  //   following order: clean aperture first, then rotation, then mirror.
  //
  // In the kModeAngleToOrientation array, the first dimension is mode (with an
  // offset of 1). The second dimension is angle.
  constexpr ImageOrientationEnum kModeAngleToOrientation[3][4] = {
      // No mirroring.
      {ImageOrientationEnum::kOriginTopLeft,
       ImageOrientationEnum::kOriginLeftBottom,
       ImageOrientationEnum::kOriginBottomRight,
       ImageOrientationEnum::kOriginRightTop},
      // Top-to-bottom mirroring. Change Top<->Bottom in the first row.
      {ImageOrientationEnum::kOriginBottomLeft,
       ImageOrientationEnum::kOriginLeftTop,
       ImageOrientationEnum::kOriginTopRight,
       ImageOrientationEnum::kOriginRightBottom},
      // Left-to-right mirroring. Change Left<->Right in the first row.
      {ImageOrientationEnum::kOriginTopRight,
       ImageOrientationEnum::kOriginRightBottom,
       ImageOrientationEnum::kOriginBottomLeft,
       ImageOrientationEnum::kOriginLeftTop},
  };
  orientation_ = kModeAngleToOrientation[mode + 1][angle];

  // Determine whether the image can be decoded to YUV.
  // * Alpha channel is not supported.
  // * Multi-frame images (animations) are not supported. (The DecodeToYUV()
  //   method does not have an 'index' parameter.)
  allow_decode_to_yuv_ =
      avif_yuv_format_ != AVIF_PIXEL_FORMAT_YUV400 && !decoder_->alphaPresent &&
      decoded_frame_count_ == 1 &&
      GetColorSpace(container).ToSkYUVColorSpace(container->depth,
                                                 &yuv_color_space_) &&
      // TODO(crbug.com/911246): Support color space transforms for YUV decodes.
      !ColorTransform();

  unsigned width = container->width;
  unsigned height = container->height;
  // If the image is cropped, pass the size of the cropped image (the clean
  // aperture) to SetSize().
  if (container->transformFlags & AVIF_TRANSFORM_CLAP) {
    AVIFCleanApertureType clap_type;
    avifCropRect crop_rect;
    avifDiagnostics diag;
    avifBool valid_clap = avifCropRectConvertCleanApertureBox(
        &crop_rect, &container->clap, container->width, container->height,
        container->yuvFormat, &diag);
    if (!valid_clap) {
      DVLOG(1) << "Invalid 'clap' property: " << diag.error
               << "; showing the full image.";
      clap_type = AVIFCleanApertureType::kInvalid;
      ignore_clap_ = true;
    } else if (crop_rect.x != 0 || crop_rect.y != 0) {
      // To help discourage the creation of files with privacy risks, also
      // consider 'clap' properties whose origins are not at (0, 0) as invalid.
      // See https://github.com/AOMediaCodec/av1-avif/issues/188 and
      // https://github.com/AOMediaCodec/av1-avif/issues/189.
      DVLOG(1) << "Origin of 'clap' property anchored to (" << crop_rect.x
               << ", " << crop_rect.y << "); showing the full image.";
      clap_type = AVIFCleanApertureType::kNonzeroOrigin;
      ignore_clap_ = true;
    } else {
      clap_type = AVIFCleanApertureType::kZeroOrigin;
      clap_origin_.SetPoint(crop_rect.x, crop_rect.y);
      width = crop_rect.width;
      height = crop_rect.height;
    }
    clap_type_ = clap_type;
  }
  return SetSize(width, height);
}

avifResult AVIFImageDecoder::DecodeImage(wtf_size_t index) {
  const auto ret = avifDecoderNthImage(decoder_.get(), index);
  // |index| should be less than what DecodeFrameCount() returns, so we should
  // not get the AVIF_RESULT_NO_IMAGES_REMAINING error.
  DCHECK_NE(ret, AVIF_RESULT_NO_IMAGES_REMAINING);
  if (ret != AVIF_RESULT_OK && ret != AVIF_RESULT_WAITING_ON_IO) {
    DVLOG(1) << "avifDecoderNthImage(" << index
             << ") failed: " << avifResultToString(ret) << ": "
             << AvifDecoderErrorMessage(decoder_.get());
    return ret;
  }

  const auto* image = decoder_->image;
  // Frame size must be equal to container size.
  if (image->width != container_width_ || image->height != container_height_) {
    DVLOG(1) << "Frame size " << image->width << "x" << image->height
             << " differs from container size " << container_width_ << "x"
             << container_height_;
    return AVIF_RESULT_UNKNOWN_ERROR;
  }
  // Frame bit depth must be equal to container bit depth.
  if (image->depth != bit_depth_) {
    DVLOG(1) << "Frame bit depth must be equal to container bit depth";
    return AVIF_RESULT_UNKNOWN_ERROR;
  }
  // Frame YUV format must be equal to container YUV format.
  if (image->yuvFormat != avif_yuv_format_) {
    DVLOG(1) << "Frame YUV format must be equal to container YUV format";
    return AVIF_RESULT_UNKNOWN_ERROR;
  }

  decoded_image_ = image;
  if ((image->transformFlags & AVIF_TRANSFORM_CLAP) && !ignore_clap_) {
    CropDecodedImage();
  }

  if (ret == AVIF_RESULT_OK && clap_type_.has_value()) {
    base::UmaHistogramEnumeration("Blink.ImageDecoders.Avif.CleanAperture",
                                  clap_type_.value());
    clap_type_.reset();
  }
  return ret;
}

void AVIFImageDecoder::UpdateColorTransform(const gfx::ColorSpace& frame_cs,
                                            int bit_depth) {
  if (color_transform_ && color_transform_->GetSrcColorSpace() == frame_cs)
    return;

  // For YUV-to-RGB color conversion we can pass an invalid dst color space to
  // skip the code for full color conversion.
  gfx::ColorTransform::Options options;
  options.src_bit_depth = bit_depth;
  options.dst_bit_depth = bit_depth;
  color_transform_ = gfx::ColorTransform::NewColorTransform(
      frame_cs, gfx::ColorSpace(), options);
}

void AVIFImageDecoder::CropDecodedImage() {
  DCHECK_NE(decoded_image_, cropped_image_.get());
  if (!cropped_image_) {
    cropped_image_.reset(avifImageCreateEmpty());
  }
  avifCropRect rect;
  rect.x = clap_origin_.x();
  rect.y = clap_origin_.y();
  rect.width = Size().width();
  rect.height = Size().height();
  const avifResult result =
      avifImageSetViewRect(cropped_image_.get(), decoded_image_, &rect);
  CHECK_EQ(result, AVIF_RESULT_OK);
  decoded_image_ = cropped_image_.get();
}

bool AVIFImageDecoder::RenderImage(const avifImage* image,
                                   int from_row,
                                   int* to_row,
                                   ImageFrame* buffer) {
  const gfx::ColorSpace frame_cs = GetColorSpace(image);
  const bool is_mono = image->yuvFormat == AVIF_PIXEL_FORMAT_YUV400;
  const bool premultiply_alpha = buffer->PremultiplyAlpha();

  DCHECK_LT(from_row, *to_row);

  // media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels() uses
  // libyuv for the YUV 4:2:0 to RGB upsampling and conversion as follows:
  //  - convert the top RGB row 0,
  //  - convert the RGB rows 1 and 2, then RGB rows 3 and 4 etc.,
  //  - convert the bottom (odd) RGB row if there is an even number of RGB rows.
  //
  // Unfortunately this cannot be applied incrementally as is. The RGB values
  // would differ because the first and last RGB rows have a formula using only
  // one UV row, while the other RGB rows use two UV rows as input each.
  // See https://crbug.com/libyuv/934.
  //
  // The workaround is a backup of the last converted even RGB row, called top
  // row, located right before |from_row|. The conversion is then called
  // starting at this top row, overwriting it with invalid values. The remaining
  // pairs of rows are correctly aligned and their freshly converted values are
  // valid. Then the backed up row is put back, fixing the issue.
  // The bottom row is postponed if the other half of the pair it belongs to is
  // not yet decoded.
  //
  //  UV rows |                 Y/RGB rows
  //          |  all  |  first decoding  |  second decoding
  //           ____ 0  ____ 0 (from_row)
  //    0 ---- ____ 1  ____ 1
  //           ____ 2  ____ 2             ____ 2 (backed up)
  //    1 ---- ____ 3  ____ 3 (postponed) ____ 3 (from_row)
  //           ____ 4       4 (*to_row)   ____ 4
  //    2 ---- ____ 5                     ____ 5
  //                                           6 (*to_row)

  const bool use_libyuv_bilinear_upsampling =
      !decode_to_half_float_ && IsColorSpaceSupportedByPCVR(image) &&
      image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420;
  const bool save_top_row = use_libyuv_bilinear_upsampling && from_row > 0;
  const bool postpone_bottom_row =
      use_libyuv_bilinear_upsampling &&
      static_cast<uint32_t>(*to_row) < image->height;
  if (postpone_bottom_row) {
    // libavif outputs an even number of rows because 4:2:0 samples are decoded
    // in pairs.
    DCHECK(!(*to_row & 1));
    --*to_row;
    if (from_row == *to_row) {
      return true;  // Nothing to do.
    }
  }
  if (save_top_row) {
    // |from_row| is odd because it is equal to the output value of |*to_row|
    // from the previous RenderImage() call, and |*to_row| was even and then
    // decremented at that time.
    DCHECK(from_row & 1);
    --from_row;
  }

  // Focus |image| on rows [from_row, *to_row).
  std::unique_ptr<avifImage, decltype(&avifImageDestroy)> view(
      nullptr, avifImageDestroy);
  if (from_row > 0 || static_cast<uint32_t>(*to_row) < image->height) {
    const avifCropRect rect = {0, static_cast<uint32_t>(from_row), image->width,
                               static_cast<uint32_t>(*to_row - from_row)};
    view.reset(avifImageCreateEmpty());
    const avifResult result = avifImageSetViewRect(view.get(), image, &rect);
    CHECK_EQ(result, AVIF_RESULT_OK);
    image = view.get();
  }

  if (decode_to_half_float_) {
    UpdateColorTransform(frame_cs, image->depth);

    uint64_t* rgba_hhhh = buffer->GetAddrF16(0, from_row);

    // Color and format convert from YUV HBD -> RGBA half float.
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint16_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_hhhh);
    } else {
      // TODO(crbug.com/1099820): Add fast path for 10bit 4:2:0 using libyuv.
      YUVAToRGBA<ColorType::kColor, uint16_t>(image, color_transform_.get(),
                                              premultiply_alpha, rgba_hhhh);
    }
    return true;
  }

  uint32_t* rgba_8888 = buffer->GetAddr(0, from_row);
  // Call media::PaintCanvasVideoRenderer (PCVR) if the color space is
  // supported.
  if (IsColorSpaceSupportedByPCVR(image)) {
    // Create temporary frame wrapping the YUVA planes.
    const bool has_alpha = image->alphaPlane != nullptr;
    auto pixel_format =
        AvifToVideoPixelFormat(image->yuvFormat, has_alpha, image->depth);
    if (pixel_format == media::PIXEL_FORMAT_UNKNOWN)
      return false;
    auto size = gfx::Size(image->width, image->height);
    scoped_refptr<media::VideoFrame> frame;
    if (has_alpha) {
      frame = media::VideoFrame::WrapExternalYuvaData(
          pixel_format, size, gfx::Rect(size), size, image->yuvRowBytes[0],
          image->yuvRowBytes[1], image->yuvRowBytes[2], image->alphaRowBytes,
          image->yuvPlanes[0], image->yuvPlanes[1], image->yuvPlanes[2],
          image->alphaPlane, base::TimeDelta());
    } else {
      frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, image->yuvRowBytes[0],
          image->yuvRowBytes[1], image->yuvRowBytes[2], image->yuvPlanes[0],
          image->yuvPlanes[1], image->yuvPlanes[2], base::TimeDelta());
    }
    if (!frame)
      return false;
    frame->set_color_space(frame_cs);

    if (save_top_row) {
      previous_last_decoded_row_.resize(image->width);
      std::copy(rgba_8888, rgba_8888 + image->width,
                previous_last_decoded_row_.begin());
    }

    // Really only handles 709, 601, 2020, JPEG 8-bit conversions and uses
    // libyuv under the hood, so is much faster than our manual path.
    //
    // Technically has support for 10-bit 4:2:0 and 4:2:2, but not to
    // half-float and only has support for 4:4:4 and 12-bit by down-shifted
    // copies.
    //
    // https://bugs.chromium.org/p/libyuv/issues/detail?id=845
    media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        frame.get(), rgba_8888, frame->visible_rect().width() * 4,
        premultiply_alpha, media::PaintCanvasVideoRenderer::kFilterBilinear,
        /*disable_threading=*/true);

    if (save_top_row) {
      base::ranges::copy(previous_last_decoded_row_, rgba_8888);
    }
    return true;
  }

  UpdateColorTransform(frame_cs, image->depth);
  if (ImageIsHighBitDepth()) {
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint16_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_8888);
    } else {
      YUVAToRGBA<ColorType::kColor, uint16_t>(image, color_transform_.get(),
                                              premultiply_alpha, rgba_8888);
    }
  } else {
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint8_t>(image, color_transform_.get(),
                                            premultiply_alpha, rgba_8888);
    } else {
      YUVAToRGBA<ColorType::kColor, uint8_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_8888);
    }
  }
  return true;
}

void AVIFImageDecoder::ColorCorrectImage(int from_row,
                                         int to_row,
                                         ImageFrame* buffer) {
  // Postprocess the image data according to the profile.
  const ColorProfileTransform* const transform = ColorTransform();
  if (!transform)
    return;
  const auto alpha_format = (buffer->HasAlpha() && buffer->PremultiplyAlpha())
                                ? skcms_AlphaFormat_PremulAsEncoded
                                : skcms_AlphaFormat_Unpremul;
  if (decode_to_half_float_) {
    const skcms_PixelFormat color_format = skcms_PixelFormat_RGBA_hhhh;
    for (int y = from_row; y < to_row; ++y) {
      ImageFrame::PixelDataF16* const row = buffer->GetAddrF16(0, y);
      const bool success = skcms_Transform(
          row, color_format, alpha_format, transform->SrcProfile(), row,
          color_format, alpha_format, transform->DstProfile(), Size().width());
      DCHECK(success);
    }
  } else {
    const skcms_PixelFormat color_format = XformColorFormat();
    for (int y = from_row; y < to_row; ++y) {
      ImageFrame::PixelData* const row = buffer->GetAddr(0, y);
      const bool success = skcms_Transform(
          row, color_format, alpha_format, transform->SrcProfile(), row,
          color_format, alpha_format, transform->DstProfile(), Size().width());
      DCHECK(success);
    }
  }
}

}  // namespace blink
