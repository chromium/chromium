/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

#include <memory>

#include "base/numerics/checked_math.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
#include <arm_neon.h>
#endif

namespace blink {

PNGImageDecoder::PNGImageDecoder(
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    const ColorBehavior& color_behavior,
    size_t max_decoded_bytes,
    size_t offset)
    : ImageDecoder(alpha_option,
                   high_bit_depth_decoding_option,
                   color_behavior,
                   max_decoded_bytes),
      offset_(offset),
      current_frame_(0),
      // It would be logical to default to kAnimationNone, but BitmapImage uses
      // that as a signal to never check again, meaning the actual count will
      // never be respected.
      repetition_count_(kAnimationLoopOnce),
      has_alpha_channel_(false),
      current_buffer_saw_alpha_(false),
      decode_to_half_float_(false),
      bit_depth_(0) {}

PNGImageDecoder::~PNGImageDecoder() = default;

bool PNGImageDecoder::SetFailed() {
  reader_.reset();
  return ImageDecoder::SetFailed();
}

size_t PNGImageDecoder::DecodeFrameCount() {
  Parse(ParseQuery::kMetaData);
  return Failed() ? frame_buffer_cache_.size() : reader_->FrameCount();
}

void PNGImageDecoder::Decode(size_t index) {
  Parse(ParseQuery::kMetaData);

  if (Failed())
    return;

  UpdateAggressivePurging(index);

  Vector<size_t> frames_to_decode = FindFramesToDecode(index);
  for (auto i = frames_to_decode.rbegin(); i != frames_to_decode.rend(); i++) {
    current_frame_ = *i;
    if (!reader_->Decode(*data_, *i)) {
      SetFailed();
      return;
    }

    // If this returns false, we need more data to continue decoding.
    if (!PostDecodeProcessing(*i))
      break;
  }

  // It is also a fatal error if all data is received and we have decoded all
  // frames available but the file is truncated.
  if (index >= frame_buffer_cache_.size() - 1 && IsAllDataReceived() &&
      reader_ && !reader_->ParseCompleted())
    SetFailed();
}

void PNGImageDecoder::Parse(ParseQuery query) {
  if (Failed() || (reader_ && reader_->ParseCompleted()))
    return;

  if (!reader_)
    reader_ = std::make_unique<PNGImageReader>(this, offset_);

  if (!reader_->Parse(*data_, query))
    SetFailed();
}

void PNGImageDecoder::ClearFrameBuffer(size_t index) {
  if (reader_)
    reader_->ClearDecodeState(index);
  ImageDecoder::ClearFrameBuffer(index);
}

bool PNGImageDecoder::CanReusePreviousFrameBuffer(size_t index) const {
  DCHECK(index < frame_buffer_cache_.size());
  return frame_buffer_cache_[index].GetDisposalMethod() !=
         ImageFrame::kDisposeOverwritePrevious;
}

void PNGImageDecoder::SetRepetitionCount(int repetition_count) {
  repetition_count_ = repetition_count;
}

int PNGImageDecoder::RepetitionCount() const {
  return Failed() ? kAnimationLoopOnce : repetition_count_;
}

void PNGImageDecoder::InitializeNewFrame(size_t index) {
  const PNGImageReader::FrameInfo& frame_info = reader_->GetFrameInfo(index);
  ImageFrame& buffer = frame_buffer_cache_[index];
  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);

  DCHECK(IntRect(IntPoint(), Size()).Contains(frame_info.frame_rect));
  buffer.SetOriginalFrameRect(frame_info.frame_rect);

  buffer.SetDuration(base::TimeDelta::FromMilliseconds(frame_info.duration));
  buffer.SetDisposalMethod(frame_info.disposal_method);
  buffer.SetAlphaBlendSource(frame_info.alpha_blend);

  size_t previous_frame_index = FindRequiredPreviousFrame(index, false);
  buffer.SetRequiredPreviousFrameIndex(previous_frame_index);
}

inline std::unique_ptr<ColorProfile> ReadColorProfile(png_structp png,
                                                      png_infop info) {
  if (png_get_valid(png, info, PNG_INFO_sRGB)) {
    return std::make_unique<ColorProfile>(*skcms_sRGB_profile());
  }

  png_charp name;
  int compression;
  png_bytep buffer;
  png_uint_32 length;
  if (png_get_iCCP(png, info, &name, &compression, &buffer, &length)) {
    return ColorProfile::Create(buffer, length);
  }

  png_fixed_point chrm[8];
  if (!png_get_cHRM_fixed(png, info, &chrm[0], &chrm[1], &chrm[2], &chrm[3],
                          &chrm[4], &chrm[5], &chrm[6], &chrm[7]))
    return nullptr;

  png_fixed_point inverse_gamma;
  if (!png_get_gAMA_fixed(png, info, &inverse_gamma))
    return nullptr;

  // cHRM and gAMA tags are both present. The PNG spec states that cHRM is
  // valid even without gAMA but we cannot apply the cHRM without guessing
  // a gAMA. Color correction is not a guessing game: match the behavior
  // of Safari and Firefox instead (compat).

  struct pngFixedToFloat {
    explicit pngFixedToFloat(png_fixed_point value)
        : float_value(.00001f * value) {}
    operator float() { return float_value; }
    float float_value;
  };

  float rx = pngFixedToFloat(chrm[2]);
  float ry = pngFixedToFloat(chrm[3]);
  float gx = pngFixedToFloat(chrm[4]);
  float gy = pngFixedToFloat(chrm[5]);
  float bx = pngFixedToFloat(chrm[6]);
  float by = pngFixedToFloat(chrm[7]);
  float wx = pngFixedToFloat(chrm[0]);
  float wy = pngFixedToFloat(chrm[1]);
  skcms_Matrix3x3 to_xyzd50;
  if (!skcms_PrimariesToXYZD50(rx, ry, gx, gy, bx, by, wx, wy, &to_xyzd50))
    return nullptr;

  skcms_TransferFunction fn;
  fn.g = 1.0f / pngFixedToFloat(inverse_gamma);
  fn.a = 1.0f;
  fn.b = fn.c = fn.d = fn.e = fn.f = 0.0f;

  skcms_ICCProfile profile;
  skcms_Init(&profile);
  skcms_SetTransferFunction(&profile, &fn);
  skcms_SetXYZD50(&profile, &to_xyzd50);

  return std::make_unique<ColorProfile>(profile);
}

void PNGImageDecoder::SetColorSpace() {
  if (IgnoresColorSpace())
    return;
  png_structp png = reader_->PngPtr();
  png_infop info = reader_->InfoPtr();
  const int color_type = png_get_color_type(png, info);
  if (!(color_type & PNG_COLOR_MASK_COLOR))
    return;
  // We only support color profiles for color PALETTE and RGB[A] PNG.
  // TODO(msarett): Add GRAY profile support, block CYMK?
  if (auto profile = ReadColorProfile(png, info)) {
    SetEmbeddedColorProfile(std::move(profile));
  }
}

void PNGImageDecoder::SetBitDepth() {
  if (bit_depth_)
    return;
  png_structp png = reader_->PngPtr();
  png_infop info = reader_->InfoPtr();
  bit_depth_ = png_get_bit_depth(png, info);
  decode_to_half_float_ =
      (bit_depth_ == 16) &&
      (high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) &&
      // TODO(zakerinasab): https://crbug.com/874057
      // Due to a lack of 16 bit APNG encoders, multi-frame 16 bit APNGs are not
      // supported. In this case the decoder falls back to 8888 decode mode.
      (repetition_count_ == kAnimationNone);
}

bool PNGImageDecoder::ImageIsHighBitDepth() {
  SetBitDepth();
  return bit_depth_ == 16;
}

bool PNGImageDecoder::SetSize(unsigned width, unsigned height) {
  DCHECK(!IsDecodedSizeAvailable());
  // Protect against large PNGs. See http://bugzil.la/251381 for more details.
  const uint32_t kMaxPNGSize = 1000000;
  return (width <= kMaxPNGSize) && (height <= kMaxPNGSize) &&
         ImageDecoder::SetSize(width, height);
}

void PNGImageDecoder::HeaderAvailable() {
  DCHECK(IsDecodedSizeAvailable());

  png_structp png = reader_->PngPtr();
  png_infop info = reader_->InfoPtr();

  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type, compression_type;
  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
               &interlace_type, &compression_type, nullptr);

  // The options we set here match what Mozilla does.

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png);

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_expand(png);

  if (!decode_to_half_float_)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  if (!HasEmbeddedColorProfile()) {
    const double kInverseGamma = 0.45455;
    const double kDefaultGamma = 2.2;
    double gamma;
    if (!IgnoresColorSpace() && png_get_gAMA(png, info, &gamma)) {
      const double kMaxGamma = 21474.83;
      if ((gamma <= 0.0) || (gamma > kMaxGamma)) {
        gamma = kInverseGamma;
        png_set_gAMA(png, info, gamma);
      }
      png_set_gamma(png, kDefaultGamma, gamma);
    } else {
      png_set_gamma(png, kDefaultGamma, kInverseGamma);
    }
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png);

  // Update our info now (so we can get color channel info).
  png_read_update_info(png, info);

  int channels = png_get_channels(png, info);
  DCHECK(channels == 3 || channels == 4);
  has_alpha_channel_ = (channels == 4);
}

#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
// Premultiply RGB color channels by alpha, swizzle RGBA to SkPMColor
// order, and return the AND of all alpha channels.
static inline void SetRGBAPremultiplyRowNeon(png_bytep src_ptr,
                                             const int pixel_count,
                                             ImageFrame::PixelData* dst_pixel,
                                             unsigned* const alpha_mask) {
  assert(dst_pixel);
  assert(alpha_mask);

  constexpr int kPixelsPerLoad = 8;
  // Input registers.
  uint8x8x4_t rgba;
  // Alpha mask.
  uint8x8_t alpha_mask_vector = vdup_n_u8(255);

  // Scale the color channel by alpha - the opacity coefficient.
  auto premultiply = [](uint8x8_t c, uint8x8_t a) {
    // First multiply the color by alpha, expanding to 16-bit (max 255*255).
    uint16x8_t ca = vmull_u8(c, a);
    // Now we need to round back down to 8-bit, returning (x+127)/255.
    // (x+127)/255 == (x + ((x+128)>>8) + 128)>>8.  This form is well suited
    // to NEON: vrshrq_n_u16(...,8) gives the inner (x+128)>>8, and
    // vraddhn_u16() both the outer add-shift and our conversion back to 8-bit.
    return vraddhn_u16(ca, vrshrq_n_u16(ca, 8));
  };

  int i = pixel_count;
  for (; i >= kPixelsPerLoad; i -= kPixelsPerLoad) {
    // Reads 8 pixels at once, each color channel in a different
    // 64-bit register.
    rgba = vld4_u8(src_ptr);
    // AND pixel alpha values into the alpha detection mask.
    alpha_mask_vector = vand_u8(alpha_mask_vector, rgba.val[3]);

    uint64_t alphas_u64 = vget_lane_u64(vreinterpret_u64_u8(rgba.val[3]), 0);

    // If all of the pixels are opaque, no need to premultiply.
    if (~alphas_u64 == 0) {
#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
      // Already in right order, write back (interleaved) results to memory.
      vst4_u8(reinterpret_cast<uint8_t*>(dst_pixel), rgba);

#elif SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
      // Re-order color channels for BGRA.
      uint8x8x4_t bgra = {rgba.val[2], rgba.val[1], rgba.val[0], rgba.val[3]};
      // Write back (interleaved) results to memory.
      vst4_u8(reinterpret_cast<uint8_t*>(dst_pixel), bgra);

#endif

    } else {
#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
      // Premultiply color channels, already in right order.
      rgba.val[0] = premultiply(rgba.val[0], rgba.val[3]);
      rgba.val[1] = premultiply(rgba.val[1], rgba.val[3]);
      rgba.val[2] = premultiply(rgba.val[2], rgba.val[3]);
      // Write back (interleaved) results to memory.
      vst4_u8(reinterpret_cast<uint8_t*>(dst_pixel), rgba);

#elif SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
      uint8x8x4_t bgra;
      // Premultiply and re-order color channels for BGRA.
      bgra.val[0] = premultiply(rgba.val[2], rgba.val[3]);
      bgra.val[1] = premultiply(rgba.val[1], rgba.val[3]);
      bgra.val[2] = premultiply(rgba.val[0], rgba.val[3]);
      bgra.val[3] = rgba.val[3];
      // Write back (interleaved) results to memory.
      vst4_u8(reinterpret_cast<uint8_t*>(dst_pixel), bgra);

#endif
    }

    // Advance to next elements.
    src_ptr += kPixelsPerLoad * 4;
    dst_pixel += kPixelsPerLoad;
  }

  // AND together the 8 alpha values in the alpha_mask_vector.
  uint64_t alpha_mask_u64 =
      vget_lane_u64(vreinterpret_u64_u8(alpha_mask_vector), 0);
  alpha_mask_u64 &= (alpha_mask_u64 >> 32);
  alpha_mask_u64 &= (alpha_mask_u64 >> 16);
  alpha_mask_u64 &= (alpha_mask_u64 >> 8);
  *alpha_mask &= alpha_mask_u64;

  // Handle the tail elements.
  for (; i > 0; i--, dst_pixel++, src_ptr += 4) {
    ImageFrame::SetRGBAPremultiply(dst_pixel, src_ptr[0], src_ptr[1],
                                   src_ptr[2], src_ptr[3]);
    *alpha_mask &= src_ptr[3];
  }
}

// Swizzle RGBA to SkPMColor order, and return the AND of all alpha channels.
static inline void SetRGBARawRowNeon(png_bytep src_ptr,
                                     const int pixel_count,
                                     ImageFrame::PixelData* dst_pixel,
                                     unsigned* const alpha_mask) {
  assert(dst_pixel);
  assert(alpha_mask);

  constexpr int kPixelsPerLoad = 16;
  // Input registers.
  uint8x16x4_t rgba;
  // Alpha mask.
  uint8x16_t alpha_mask_vector = vdupq_n_u8(255);

  int i = pixel_count;
  for (; i >= kPixelsPerLoad; i -= kPixelsPerLoad) {
    // Reads 16 pixels at once, each color channel in a different
    // 128-bit register.
    rgba = vld4q_u8(src_ptr);
    // AND pixel alpha values into the alpha detection mask.
    alpha_mask_vector = vandq_u8(alpha_mask_vector, rgba.val[3]);

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    // Already in right order, write back (interleaved) results to memory.
    vst4q_u8(reinterpret_cast<uint8_t*>(dst_pixel), rgba);

#elif SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
    // Re-order color channels for BGRA.
    uint8x16x4_t bgra = {rgba.val[2], rgba.val[1], rgba.val[0], rgba.val[3]};
    // Write back (interleaved) results to memory.
    vst4q_u8(reinterpret_cast<uint8_t*>(dst_pixel), bgra);

#endif

    // Advance to next elements.
    src_ptr += kPixelsPerLoad * 4;
    dst_pixel += kPixelsPerLoad;
  }

  // AND together the 16 alpha values in the alpha_mask_vector.
  uint64_t alpha_mask_u64 =
      vget_lane_u64(vreinterpret_u64_u8(vget_low_u8(alpha_mask_vector)), 0);
  alpha_mask_u64 &=
      vget_lane_u64(vreinterpret_u64_u8(vget_high_u8(alpha_mask_vector)), 0);
  alpha_mask_u64 &= (alpha_mask_u64 >> 32);
  alpha_mask_u64 &= (alpha_mask_u64 >> 16);
  alpha_mask_u64 &= (alpha_mask_u64 >> 8);
  *alpha_mask &= alpha_mask_u64;

  // Handle the tail elements.
  for (; i > 0; i--, dst_pixel++, src_ptr += 4) {
    ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2],
                           src_ptr[3]);
    *alpha_mask &= src_ptr[3];
  }
}

// Swizzle RGB to opaque SkPMColor order, and return the AND
// of all alpha channels.
static inline void SetRGBARawRowNoAlphaNeon(png_bytep src_ptr,
                                            const int pixel_count,
                                            ImageFrame::PixelData* dst_pixel) {
  assert(dst_pixel);

  constexpr int kPixelsPerLoad = 16;
  // Input registers.
  uint8x16x3_t rgb;

  int i = pixel_count;
  for (; i >= kPixelsPerLoad; i -= kPixelsPerLoad) {
    // Reads 16 pixels at once, each color channel in a different
    // 128-bit register.
    rgb = vld3q_u8(src_ptr);

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    // RGB already in right order, add opaque alpha channel.
    uint8x16x4_t rgba = {rgb.val[0], rgb.val[1], rgb.val[2], vdupq_n_u8(255)};
    // Write back (interleaved) results to memory.
    vst4q_u8(reinterpret_cast<uint8_t*>(dst_pixel), rgba);

#elif SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
    // Re-order color channels for BGR, add opaque alpha channel.
    uint8x16x4_t bgra = {rgb.val[2], rgb.val[1], rgb.val[0], vdupq_n_u8(255)};
    // Write back (interleaved) results to memory.
    vst4q_u8(reinterpret_cast<uint8_t*>(dst_pixel), bgra);

#endif

    // Advance to next elements.
    src_ptr += kPixelsPerLoad * 3;
    dst_pixel += kPixelsPerLoad;
  }

  // Handle the tail elements.
  for (; i > 0; i--, dst_pixel++, src_ptr += 3) {
    ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2], 255);
  }
}
#endif

void PNGImageDecoder::RowAvailable(unsigned char* row_buffer,
                                   unsigned row_index,
                                   int) {
  if (current_frame_ >= frame_buffer_cache_.size())
    return;

  ImageFrame& buffer = frame_buffer_cache_[current_frame_];
  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    png_structp png = reader_->PngPtr();
    if (!InitFrameBuffer(current_frame_)) {
      longjmp(JMPBUF(png), 1);
      return;
    }

    DCHECK_EQ(ImageFrame::kFramePartial, buffer.GetStatus());

    if (PNG_INTERLACE_ADAM7 ==
        png_get_interlace_type(png, reader_->InfoPtr())) {
      unsigned color_channels = has_alpha_channel_ ? 4 : 3;
      base::CheckedNumeric<int> interlace_buffer_size = color_channels;
      interlace_buffer_size *= Size().Area();
      if (decode_to_half_float_)
        interlace_buffer_size *= 2;
      if (!interlace_buffer_size.IsValid()) {
        longjmp(JMPBUF(png), 1);
        return;
      }
      reader_->CreateInterlaceBuffer(interlace_buffer_size.ValueOrDie());
      if (!reader_->InterlaceBuffer()) {
        longjmp(JMPBUF(png), 1);
        return;
      }
    }

    current_buffer_saw_alpha_ = false;
  }

  const IntRect& frame_rect = buffer.OriginalFrameRect();
  DCHECK(IntRect(IntPoint(), Size()).Contains(frame_rect));

  /* libpng comments (here to explain what follows).
   *
   * this function is called for every row in the image. If the
   * image is interlacing, and you turned on the interlace handler,
   * this function will be called for every row in every pass.
   * Some of these rows will not be changed from the previous pass.
   * When the row is not changed, the new_row variable will be NULL.
   * The rows and passes are called in order, so you don't really
   * need the row_num and pass, but I'm supplying them because it
   * may make your life easier.
   */

  // Nothing to do if the row is unchanged, or the row is outside the image
  // bounds. In the case that a frame presents more data than the indicated
  // frame size, ignore the extra rows and use the frame size as the source
  // of truth. libpng can send extra rows: ignore them too, this to prevent
  // memory writes outside of the image bounds (security).
  if (!row_buffer)
    return;

  DCHECK_GT(frame_rect.Height(), 0);
  if (row_index >= static_cast<unsigned>(frame_rect.Height()))
    return;

  int y = row_index + frame_rect.Y();
  if (y < 0)
    return;
  DCHECK_LT(y, Size().Height());

  /* libpng comments (continued).
   *
   * For the non-NULL rows of interlaced images, you must call
   * png_progressive_combine_row() passing in the row and the
   * old row.  You can call this function for NULL rows (it will
   * just return) and for non-interlaced images (it just does the
   * memcpy for you) if it will make the code easier. Thus, you
   * can just do this for all cases:
   *
   *    png_progressive_combine_row(png_ptr, old_row, new_row);
   *
   * where old_row is what was displayed for previous rows. Note
   * that the first pass (pass == 0 really) will completely cover
   * the old row, so the rows do not have to be initialized. After
   * the first pass (and only for interlaced images), you will have
   * to pass the current row, and the function will combine the
   * old row and the new row.
   */

  bool has_alpha = has_alpha_channel_;
  png_bytep row = row_buffer;

  if (png_bytep interlace_buffer = reader_->InterlaceBuffer()) {
    unsigned bytes_per_pixel = has_alpha ? 4 : 3;
    if (decode_to_half_float_)
      bytes_per_pixel *= 2;
    row = interlace_buffer + (row_index * bytes_per_pixel * Size().Width());
    png_progressive_combine_row(reader_->PngPtr(), row, row_buffer);
  }

  // Write the decoded row pixels to the frame buffer. The repetitive
  // form of the row write loops is for speed.
  const int width = frame_rect.Width();
  png_bytep src_ptr = row;

  if (!decode_to_half_float_) {
    ImageFrame::PixelData* const dst_row = buffer.GetAddr(frame_rect.X(), y);
    if (has_alpha) {
      if (ColorProfileTransform* xform = ColorTransform()) {
        ImageFrame::PixelData* xform_dst = dst_row;
        // If we're blending over the previous frame, we can't overwrite that
        // when we do the color transform. So we allocate another row of pixels
        // to hold the temporary result before blending. In all other cases,
        // we can safely transform directly to the destination buffer, then do
        // any operations in-place (premul, swizzle).
        if (frame_buffer_cache_[current_frame_].GetAlphaBlendSource() ==
            ImageFrame::kBlendAtopPreviousFrame) {
          if (!color_transform_scanline_) {
            // This buffer may be wider than necessary for this frame, but by
            // allocating the full width of the PNG, we know it will be able to
            // hold temporary data for any subsequent frame.
            color_transform_scanline_.reset(
                new ImageFrame::PixelData[Size().Width()]);
          }
          xform_dst = color_transform_scanline_.get();
        }
        skcms_PixelFormat color_format = skcms_PixelFormat_RGBA_8888;
        skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
        bool color_conversion_successful = skcms_Transform(
            src_ptr, color_format, alpha_format, xform->SrcProfile(), xform_dst,
            color_format, alpha_format, xform->DstProfile(), width);
        DCHECK(color_conversion_successful);
        src_ptr = png_bytep(xform_dst);
      }

      unsigned alpha_mask = 255;
      if (frame_buffer_cache_[current_frame_].GetAlphaBlendSource() ==
          ImageFrame::kBlendAtopBgcolor) {
        if (buffer.PremultiplyAlpha()) {
#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
          SetRGBAPremultiplyRowNeon(src_ptr, width, dst_row, &alpha_mask);
#else
          for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
               dst_pixel++, src_ptr += 4) {
            ImageFrame::SetRGBAPremultiply(dst_pixel, src_ptr[0], src_ptr[1],
                                           src_ptr[2], src_ptr[3]);
            alpha_mask &= src_ptr[3];
          }
#endif
        } else {
#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
          SetRGBARawRowNeon(src_ptr, width, dst_row, &alpha_mask);
#else
          for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
               dst_pixel++, src_ptr += 4) {
            ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1],
                                   src_ptr[2], src_ptr[3]);
            alpha_mask &= src_ptr[3];
          }
#endif
        }
      } else {
        // Now, the blend method is ImageFrame::BlendAtopPreviousFrame. Since
        // the frame data of the previous frame is copied at InitFrameBuffer, we
        // can blend the pixel of this frame, stored in |src_ptr|, over the
        // previous pixel stored in |dst_pixel|.
        if (buffer.PremultiplyAlpha()) {
          for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
               dst_pixel++, src_ptr += 4) {
            ImageFrame::BlendRGBAPremultiplied(
                dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2], src_ptr[3]);
            alpha_mask &= src_ptr[3];
          }
        } else {
          for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
               dst_pixel++, src_ptr += 4) {
            ImageFrame::BlendRGBARaw(dst_pixel, src_ptr[0], src_ptr[1],
                                     src_ptr[2], src_ptr[3]);
            alpha_mask &= src_ptr[3];
          }
        }
      }

      if (alpha_mask != 255)
        current_buffer_saw_alpha_ = true;

    } else {
#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
      SetRGBARawRowNoAlphaNeon(src_ptr, width, dst_row);
#else
      for (auto *dst_pixel = dst_row; dst_pixel < dst_row + width;
           src_ptr += 3, ++dst_pixel) {
        ImageFrame::SetRGBARaw(dst_pixel, src_ptr[0], src_ptr[1], src_ptr[2],
                               255);
      }
#endif
      // We'll apply the color space xform to opaque pixels after they have been
      // written to the ImageFrame.
      // TODO: Apply the xform to the RGB pixels, skipping second pass over
      // data.
      if (ColorProfileTransform* xform = ColorTransform()) {
        skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Opaque;
        bool color_conversion_successful =
            skcms_Transform(dst_row, XformColorFormat(), alpha_format,
                            xform->SrcProfile(), dst_row, XformColorFormat(),
                            alpha_format, xform->DstProfile(), width);
        DCHECK(color_conversion_successful);
      }
    }
  } else {  // for if (!decode_to_half_float_)
    ImageFrame::PixelDataF16* const dst_row_f16 =
        buffer.GetAddrF16(frame_rect.X(), y);

    // TODO(zakerinasab): https://crbug.com/874057
    // Due to a lack of 16 bit APNG encoders, multi-frame 16 bit APNGs are not
    // supported. Hence, we expect the blending mode always be
    // kBlendAtopBgcolor.
    DCHECK(frame_buffer_cache_[current_frame_].GetAlphaBlendSource() ==
           ImageFrame::kBlendAtopBgcolor);

    // Color space transformation to the dst space and converting the decoded
    // color componenets from uint16 to float16.
    auto* xform = ColorTransform();
    auto* src_profile = xform ? xform->SrcProfile() : nullptr;
    auto* dst_profile = xform ? xform->DstProfile() : nullptr;
    auto src_format = has_alpha ? skcms_PixelFormat_RGBA_16161616BE
                                : skcms_PixelFormat_RGB_161616BE;
    auto src_alpha_format =
        has_alpha ? skcms_AlphaFormat_Unpremul : skcms_AlphaFormat_Opaque;
    auto dst_alpha_format = has_alpha ? (buffer.PremultiplyAlpha()
                                             ? skcms_AlphaFormat_PremulAsEncoded
                                             : skcms_AlphaFormat_Unpremul)
                                      : skcms_AlphaFormat_Opaque;
    bool success = skcms_Transform(
        src_ptr, src_format, src_alpha_format, src_profile, dst_row_f16,
        skcms_PixelFormat_RGBA_hhhh, dst_alpha_format, dst_profile, width);
    DCHECK(success);

    current_buffer_saw_alpha_ = has_alpha;
  }

  buffer.SetPixelsChanged(true);
}

void PNGImageDecoder::FrameComplete() {
  if (current_frame_ >= frame_buffer_cache_.size())
    return;

  if (reader_->InterlaceBuffer())
    reader_->ClearInterlaceBuffer();

  ImageFrame& buffer = frame_buffer_cache_[current_frame_];
  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    longjmp(JMPBUF(reader_->PngPtr()), 1);
    return;
  }

  if (!current_buffer_saw_alpha_)
    CorrectAlphaWhenFrameBufferSawNoAlpha(current_frame_);

  buffer.SetStatus(ImageFrame::kFrameComplete);
}

bool PNGImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (!IsDecodedSizeAvailable())
    return false;

  DCHECK(!Failed() && reader_);

  // For non-animated images, return ImageDecoder::FrameIsReceivedAtIndex.
  // This matches the behavior of WEBPImageDecoder.
  if (reader_->ParseCompleted() && reader_->FrameCount() == 1)
    return ImageDecoder::FrameIsReceivedAtIndex(index);

  return reader_->FrameIsReceivedAtIndex(index);
}

base::TimeDelta PNGImageDecoder::FrameDurationAtIndex(size_t index) const {
  if (index < frame_buffer_cache_.size())
    return frame_buffer_cache_[index].Duration();
  return base::TimeDelta();
}

}  // namespace blink
