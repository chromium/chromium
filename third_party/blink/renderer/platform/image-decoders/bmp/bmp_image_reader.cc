/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_reader.h"

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

namespace {

// See comments on lookup_table_addresses_ in the header.
constexpr uint8_t nBitTo8BitlookupTable[] = {
    // 1 bit
    0, 255,
    // 2 bits
    0, 85, 170, 255,
    // 3 bits
    0, 36, 73, 109, 146, 182, 219, 255,
    // 4 bits
    0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255,
    // 5 bits
    0, 8, 16, 25, 33, 41, 49, 58, 66, 74, 82, 90, 99, 107, 115, 123, 132, 140,
    148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255,
    // 6 bits
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 45, 49, 53, 57, 61, 65, 69, 73, 77,
    81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 130, 134, 138, 142,
    146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190, 194, 198, 202,
    206, 210, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255,
    // 7 bits
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38,
    40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
    78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 110,
    112, 114, 116, 118, 120, 122, 124, 126, 129, 131, 133, 135, 137, 139, 141,
    143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171,
    173, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201,
    203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231,
    233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
};

}  // namespace

namespace blink {

BMPImageReader::BMPImageReader(ImageDecoder* parent,
                               size_t decoded_and_header_offset,
                               size_t img_data_offset,
                               bool is_in_ico)
    : parent_(parent),
      decoded_offset_(decoded_and_header_offset),
      header_offset_(decoded_and_header_offset),
      img_data_offset_(img_data_offset),
      is_in_ico_(is_in_ico) {
  // Clue-in decodeBMP() that we need to detect the correct info header size.
  memset(&info_header_, 0, sizeof(info_header_));
}

BMPImageReader::~BMPImageReader() = default;

void BMPImageReader::SetData(SegmentReader* data) {
  data_ = data;
  fast_reader_.SetData(data);
  if (alternate_decoder_)
    alternate_decoder_->SetData(data_.get(), parent_->IsAllDataReceived());
}

bool BMPImageReader::DecodeBMP(bool only_size) {
  // Defensively clear the FastSharedBufferReader's cache, as another caller
  // may have called SharedBuffer::MergeSegmentsIntoBuffer().
  fast_reader_.ClearCache();

  // Calculate size of info header.
  if (!info_header_.size && !ReadInfoHeaderSize())
    return false;

  const size_t header_end = header_offset_ + info_header_.size;
  // Read and process info header.
  if ((decoded_offset_ < header_end) && !ProcessInfoHeader())
    return false;

  // If there is an applicable color profile, it must be processed now, since
  // once the image size is available, the decoding machinery assumes the color
  // space is as well.  Unfortunately, since the profile appears after
  // everything else, this may delay processing until all data is received.
  // Luckily, few BMPs have an embedded color profile.
  const bool use_alternate_decoder =
      (info_header_.compression == JPEG) || (info_header_.compression == PNG);
  if (!use_alternate_decoder && info_header_.profile_data &&
      !ProcessEmbeddedColorProfile())
    return false;

  // Set our size if we haven't already.  In ICO files, IsDecodedSizeAvailable()
  // always returns true (since it reflects the size in the directory, which has
  // already been read); call SetSize() anyway, since it sanity-checks that the
  // size here matches the directory.
  if ((is_in_ico_ || !parent_->IsDecodedSizeAvailable()) &&
      !parent_->SetSize(info_header_.width, info_header_.height))
    return false;

  if (only_size)
    return true;

  if (use_alternate_decoder)
    return DecodeAlternateFormat();

  // Read and process the bitmasks, if needed.
  if (need_to_process_bitmasks_ && !ProcessBitmasks())
    return false;

  // Read and process the color table, if needed.
  if (need_to_process_color_table_ && !ProcessColorTable())
    return false;

  // Initialize the framebuffer if needed.
  DCHECK(buffer_);  // Parent should set this before asking us to decode!
  if ((buffer_->GetStatus() == ImageFrame::kFrameEmpty) && !InitFrame())
    return false;

  // Decode the data.
  if (!decoding_and_mask_ && !PastEndOfImage(0) &&
      !DecodePixelData((info_header_.compression != RLE4) &&
                       (info_header_.compression != RLE8) &&
                       (info_header_.compression != RLE24)))
    return false;

  // If the image has an AND mask and there was no alpha data, process the
  // mask.
  if (is_in_ico_ && !decoding_and_mask_ &&
      ((info_header_.bit_count < 16) || !bit_masks_[3] ||
       !seen_non_zero_alpha_pixel_)) {
    // Reset decoding coordinates to start of image.
    coord_.SetX(0);
    coord_.SetY(is_top_down_ ? 0 : (parent_->Size().Height() - 1));

    // The AND mask is stored as 1-bit data.
    info_header_.bit_count = 1;

    decoding_and_mask_ = true;
  }
  if (decoding_and_mask_ && !DecodePixelData(true))
    return false;

  // Done!
  buffer_->SetStatus(ImageFrame::kFrameComplete);
  return true;
}

bool BMPImageReader::ReadInfoHeaderSize() {
  // Get size of info header.
  DCHECK_EQ(decoded_offset_, header_offset_);
  if ((decoded_offset_ > data_->size()) ||
      ((data_->size() - decoded_offset_) < 4))
    return false;
  info_header_.size = ReadUint32(0);
  // Don't increment decoded_offset here, it just makes the code in
  // ProcessInfoHeader() more confusing.

  // Don't allow the header to overflow (which would be harmless here, but
  // problematic or at least confusing in other places), or to overrun the
  // image data.
  const size_t header_end = header_offset_ + info_header_.size;
  if ((header_end < header_offset_) ||
      (img_data_offset_ && (img_data_offset_ < header_end)))
    return parent_->SetFailed();

  // See if this is a header size we understand.  See comments in
  // ReadInfoHeader() for more.
  // OS/2 1.x (and Windows V2): 12
  if (info_header_.size == 12)
    is_os21x_ = true;
  // Windows V3+: 40, 52, 56, 108, 124
  else if ((info_header_.size == 40) || HasRGBMasksInHeader())
    ;
  // OS/2 2.x: any multiple of 4 between 16 and 64, inclusive, or 42 or 46
  else if ((info_header_.size >= 16) && (info_header_.size <= 64) &&
           (!(info_header_.size & 3) || (info_header_.size == 42) ||
            (info_header_.size == 46)))
    is_os22x_ = true;
  else
    return parent_->SetFailed();

  return true;
}

bool BMPImageReader::ProcessInfoHeader() {
  // Read info header.
  DCHECK_EQ(decoded_offset_, header_offset_);
  if ((decoded_offset_ > data_->size()) ||
      ((data_->size() - decoded_offset_) < info_header_.size) ||
      !ReadInfoHeader())
    return false;

  // Sanity-check header values before doing further fixup.
  if (!IsInfoHeaderValid())
    return parent_->SetFailed();

  // For paletted images, bitmaps can set clr_used to 0 to mean "all colors", so
  // set it to the maximum number of colors for this bit depth.  Also do this
  // for bitmaps that put too large a value here.
  if (info_header_.bit_count < 16) {
    const uint32_t max_colors = uint32_t{1} << info_header_.bit_count;
    if (!info_header_.clr_used || (info_header_.clr_used > max_colors))
      info_header_.clr_used = max_colors;
  }

  // For any bitmaps that set their BitCount to the wrong value, reset the
  // counts now that we've calculated the number of necessary colors, since
  // other code relies on this value being correct.
  if (info_header_.compression == RLE8)
    info_header_.bit_count = 8;
  else if (info_header_.compression == RLE4)
    info_header_.bit_count = 4;

  // Tell caller what still needs to be processed.
  if (info_header_.bit_count >= 16)
    need_to_process_bitmasks_ = true;
  else if (info_header_.bit_count)
    need_to_process_color_table_ = true;

  decoded_offset_ += info_header_.size;
  return true;
}

bool BMPImageReader::ReadInfoHeader() {
  // Supported info header formats:
  // * BITMAPCOREHEADER/OS21XBITMAPHEADER/"Windows V2".  Windows 2.x (?),
  //   OS/2 1.x.  12 bytes.  Incompatible with all below headers.
  // * BITMAPINFOHEADER/"Windows V3".  Windows 3.x.  40 bytes.  Changes width/
  //   height fields to 32 bit and adds features such as compression types.
  //   (Nomenclature: Note that "Windows V3" here and "BITMAPV3..." below are
  //   different things.)
  // * OS22XBITMAPHEADER/BITMAPCOREHEADER2.  OS/2 2.x.  16-64 bytes.  The first
  //   40 bytes are basically identical to BITMAPINFOHEADER, save that most
  //   fields are optional.  Further fields, if present, are incompatible with
  //   all below headers.  Adds features such as halftoning and color spaces
  //   (not implemented here).
  // * BITMAPV2HEADER/BITMAPV2INFOHEADER.  52 bytes.  Extends BITMAPINFOHEADER
  //   with R/G/B masks.  Poorly-documented and obscure.
  // * BITMAPV3HEADER/BITMAPV3INFOHEADER.  56 bytes.  Extends BITMAPV2HEADER
  //   with an alpha mask.  Poorly-documented and obscure.
  // * BITMAPV4HEADER/"Windows V4".  Windows 95.  108 bytes.  Extends
  //   BITMAPV3HEADER with color space support.
  // * BITMAPV5HEADER/"Windows V5".  Windows 98.  124 bytes.  Extends
  //   BITMAPV4HEADER with ICC profile support.

  // Pre-initialize some fields that not all headers set.
  info_header_.compression = RGB;
  info_header_.clr_used = 0;
  info_header_.profile_data = 0;
  info_header_.profile_size = 0;

  if (is_os21x_) {
    info_header_.width = ReadUint16(4);
    info_header_.height = ReadUint16(6);
    info_header_.bit_count = ReadUint16(10);
    return true;
  }

  info_header_.width = ReadUint32(4);
  info_header_.height = ReadUint32(8);
  if (is_in_ico_)
    info_header_.height /= 2;
  // Detect top-down BMPs.
  if (info_header_.height < 0) {
    // We can't negate INT32_MIN below to get a positive int32_t.
    // IsInfoHeaderValid() will reject heights of 1 << 16 or larger anyway,
    // so just reject this bitmap now.
    if (info_header_.height == INT32_MIN)
      return parent_->SetFailed();
    is_top_down_ = true;
    info_header_.height = -info_header_.height;
  }

  info_header_.bit_count = ReadUint16(14);

  // Read compression type, if present.
  if (info_header_.size >= 20) {
    const uint32_t compression = ReadUint32(16);

    // Detect OS/2 2.x-specific compression types.
    if ((compression == 3) && (info_header_.bit_count == 1)) {
      info_header_.compression = HUFFMAN1D;
      is_os22x_ = true;
    } else if ((compression == 4) && (info_header_.bit_count == 24)) {
      info_header_.compression = RLE24;
      is_os22x_ = true;
    } else if (compression > ALPHABITFIELDS) {
      return parent_->SetFailed();  // Some type we don't understand.
    } else {
      info_header_.compression = static_cast<CompressionType>(compression);
    }
  }

  // Read colors used, if present.
  if (info_header_.size >= 36)
    info_header_.clr_used = ReadUint32(32);

  // If we can safely read the four bitmasks from 40-56 bytes in, do that here.
  // If the bit depth is less than 16, these values will be ignored by the image
  // data decoders. If the bit depth is at least 16 but the compression format
  // isn't [ALPHA]BITFIELDS, the RGB bitmasks will be ignored and overwritten in
  // processBitmasks(). (The alpha bitmask will never be overwritten: images
  // that actually want alpha have to specify a valid alpha mask. See comments
  // in ProcessBitmasks().)
  //
  // For other BMPs, bit_masks_[] et. al will be initialized later during
  // ProcessBitmasks().
  if (HasRGBMasksInHeader()) {
    bit_masks_[0] = ReadUint32(40);
    bit_masks_[1] = ReadUint32(44);
    bit_masks_[2] = ReadUint32(48);
  }
  if (HasAlphaMaskInHeader())
    bit_masks_[3] = ReadUint32(52);

  // Read color space information, if present and desirable.
  if (HasColorSpaceInfoInHeader() && !parent_->IgnoresColorSpace()) {
    enum {
      kLcsCalibratedRGB = 0x00000000,
      kLcssRGB = 0x73524742,               // "sRGB"
      kLcsWindowsColorSpace = 0x57696E20,  // "Win "
      kProfileLinked = 0x4c494e4b,         // "LINK"
      kProfileEmbedded = 0x4d424544,       // "MBED"
    };

    const uint32_t cs_type = ReadUint32(56);
    switch (cs_type) {
      case kLcsCalibratedRGB: {  // Endpoints and gamma specified directly
        skcms_ICCProfile profile;
        skcms_Init(&profile);

        // Convert chromaticity values from 2.30 fixed point to floating point.
        const auto fxpt2dot30_to_float = [](uint32_t fxpt2dot30) {
          return fxpt2dot30 * 9.31322574615478515625e-10f;
        };
        const float rx = fxpt2dot30_to_float(ReadUint32(60));
        const float ry = fxpt2dot30_to_float(ReadUint32(64));
        const float gx = fxpt2dot30_to_float(ReadUint32(72));
        const float gy = fxpt2dot30_to_float(ReadUint32(76));
        const float bx = fxpt2dot30_to_float(ReadUint32(84));
        const float by = fxpt2dot30_to_float(ReadUint32(88));
        // BMPs do not explicitly encode a white point.  Using the sRGB
        // illuminant (D65) seems reasonable given that Windows' system color
        // space is sRGB.
        constexpr float kD65x = 0.31271;
        constexpr float kD65y = 0.32902;
        skcms_Matrix3x3 to_xyzd50;
        if (!skcms_PrimariesToXYZD50(rx, ry, gx, gy, bx, by, kD65x, kD65y,
                                     &to_xyzd50)) {
          // Some real-world images have bogus values, e.g. all zeros.  Ignore
          // the color space data in such cases, rather than failing.
          break;
        }
        skcms_SetXYZD50(&profile, &to_xyzd50);

        // Convert gamma values from 16.16 fixed point to transfer functions.
        const auto fxpt16dot16_to_fn = [](uint32_t fxpt16dot16) {
          skcms_TransferFunction fn;
          fn.a = 1.0f;
          fn.b = fn.c = fn.d = fn.e = fn.f = 0.0f;
          // Petzold's "Programming Windows" claims the gamma here is a decoding
          // gamma (e.g. 2.2), as opposed to the inverse, an encoding gamma
          // (like PNG encodes in its gAMA chunk).
          fn.g = SkFixedToFloat(fxpt16dot16);
          return fn;
        };
        profile.has_trc = true;
        profile.trc[0].table_entries = 0;
        profile.trc[0].parametric = fxpt16dot16_to_fn(ReadUint32(96));
        profile.trc[1].table_entries = 0;
        profile.trc[1].parametric = fxpt16dot16_to_fn(ReadUint32(100));
        profile.trc[2].table_entries = 0;
        profile.trc[2].parametric = fxpt16dot16_to_fn(ReadUint32(104));

        parent_->SetEmbeddedColorProfile(
            std::make_unique<ColorProfile>(profile));
        break;
      }

      case kLcssRGB:               // sRGB
      case kLcsWindowsColorSpace:  // "The Windows default color space" (sRGB)
        parent_->SetEmbeddedColorProfile(
            std::make_unique<ColorProfile>(*skcms_sRGB_profile()));
        break;

      case kProfileEmbedded:  // Embedded ICC profile
        if (info_header_.size >= 120) {
          info_header_.profile_data = header_offset_ + ReadUint32(112);
          info_header_.profile_size = ReadUint32(116);
        }
        break;

      case kProfileLinked:     // Linked ICC profile.  Unsupported; presents
                               // security concerns.
      default:                 // Unknown.
        break;
    }
  }

  return true;
}

bool BMPImageReader::IsInfoHeaderValid() const {
  // Non-positive widths/heights are invalid.  (We've already flipped the
  // sign of the height for top-down bitmaps.)
  if ((info_header_.width <= 0) || !info_header_.height)
    return false;

  // Only Windows V3+ has ICOs and top-down bitmaps.
  if ((is_in_ico_ || is_top_down_) && (is_os21x_ || is_os22x_))
    return false;

  // Only bit depths of 1, 4, 8, or 24 are universally supported.
  if ((info_header_.bit_count != 1) && (info_header_.bit_count != 4) &&
      (info_header_.bit_count != 8) && (info_header_.bit_count != 24)) {
    // Windows V3+ additionally supports bit depths of 0 (for embedded
    // JPEG/PNG images), 2 (on Windows CE), 16, and 32.
    if (is_os21x_ || is_os22x_ ||
        (info_header_.bit_count && (info_header_.bit_count != 2) &&
         (info_header_.bit_count != 16) && (info_header_.bit_count != 32)))
      return false;
  }

  // Each compression type is only valid with certain bit depths (except RGB,
  // which can be used with any bit depth). Also, some formats do not support
  // some compression types.
  switch (info_header_.compression) {
    case RGB:
      if (!info_header_.bit_count)
        return false;
      break;

    case RLE8:
      // Supposedly there are undocumented formats like "BitCount = 1,
      // Compression = RLE4" (which means "4 bit, but with a 2-color table"),
      // so also allow the paletted RLE compression types to have too low a
      // bit count; we'll correct this later.
      if (!info_header_.bit_count || (info_header_.bit_count > 8))
        return false;
      break;

    case RLE4:
      // See comments in RLE8.
      if (!info_header_.bit_count || (info_header_.bit_count > 4))
        return false;
      break;

    case BITFIELDS:
    case ALPHABITFIELDS:
      // Only valid for Windows V3+.
      if (is_os21x_ || is_os22x_ ||
          ((info_header_.bit_count != 16) && (info_header_.bit_count != 32)))
        return false;
      break;

    case JPEG:
    case PNG:
      // Only valid for Windows V3+.  We don't support embedding these inside
      // ICO files.
      if (is_os21x_ || is_os22x_ || info_header_.bit_count || !img_data_offset_)
        return false;
      break;

    case HUFFMAN1D:
      // Only valid for OS/2 2.x.
      if (!is_os22x_ || (info_header_.bit_count != 1))
        return false;
      break;

    case RLE24:
      // Only valid for OS/2 2.x.
      if (!is_os22x_ || (info_header_.bit_count != 24))
        return false;
      break;

    default:
      // Some type we don't understand.  This should have been caught in
      // ReadInfoHeader().
      NOTREACHED();
      return false;
  }

  // Reject the following valid bitmap types that we don't currently bother
  // decoding.  Few other people decode these either, they're unlikely to be
  // in much use.
  // TODO(pkasting): Consider supporting these someday.
  //   * Bitmaps larger than 2^16 pixels in either dimension.
  if ((info_header_.width >= (1 << 16)) || (info_header_.height >= (1 << 16)))
    return false;
  //   * OS/2 2.x Huffman-encoded monochrome bitmaps (see
  //      http://www.fileformat.info/mirror/egff/ch09_05.htm , re: "G31D"
  //      algorithm; this seems to be used in TIFF files as well).
  if (info_header_.compression == HUFFMAN1D)
    return false;

  return true;
}

bool BMPImageReader::DecodeAlternateFormat() {
  // Create decoder if necessary.
  if (!alternate_decoder_) {
    if (info_header_.compression == JPEG) {
      alternate_decoder_ = std::make_unique<JPEGImageDecoder>(
          parent_->GetAlphaOption(), parent_->GetColorBehavior(),
          parent_->GetMaxDecodedBytes(),
          ImageDecoder::OverrideAllowDecodeToYuv::kDefault, img_data_offset_);
    } else {
      alternate_decoder_ = std::make_unique<PNGImageDecoder>(
          parent_->GetAlphaOption(), ImageDecoder::kDefaultBitDepth,
          parent_->GetColorBehavior(), parent_->GetMaxDecodedBytes(),
          img_data_offset_);
    }
    alternate_decoder_->SetData(data_.get(), parent_->IsAllDataReceived());
  }

  // Decode the image.
  if (alternate_decoder_->IsSizeAvailable()) {
    if (alternate_decoder_->Size() != parent_->Size())
      return parent_->SetFailed();

    alternate_decoder_->SetMemoryAllocator(buffer_->GetAllocator());
    const auto* frame = alternate_decoder_->DecodeFrameBufferAtIndex(0);
    alternate_decoder_->SetMemoryAllocator(nullptr);

    if (frame)
      *buffer_ = *frame;
  }
  return alternate_decoder_->Failed()
             ? parent_->SetFailed()
             : (buffer_->GetStatus() == ImageFrame::kFrameComplete);
}

bool BMPImageReader::ProcessEmbeddedColorProfile() {
  // Ensure we have received the whole profile.
  if ((info_header_.profile_data > data_->size()) ||
      ((data_->size() - info_header_.profile_data) < info_header_.profile_size))
    return false;

  // Parse the profile.
  auto owned_buffer = std::make_unique<char[]>(info_header_.profile_size);
  const char* buffer = fast_reader_.GetConsecutiveData(
      info_header_.profile_data, info_header_.profile_size, owned_buffer.get());
  auto profile = ColorProfile::Create(buffer, info_header_.profile_size);
  if (!profile)
    return parent_->SetFailed();
  parent_->SetEmbeddedColorProfile(std::move(profile));

  // Zero |profile_data| so we don't try to process the profile again in the
  // future.
  info_header_.profile_data = 0;
  return true;
}

bool BMPImageReader::ProcessBitmasks() {
  // Create bit_masks_[] values for R/G/B.
  if ((info_header_.compression != BITFIELDS) &&
      (info_header_.compression != ALPHABITFIELDS)) {
    // The format doesn't actually use bitmasks.  To simplify the decode
    // logic later, create bitmasks for the RGB data.  For Windows V4+,
    // this overwrites the masks we read from the header, which are
    // supposed to be ignored in non-BITFIELDS cases.
    // 16 bits:    MSB <-                     xRRRRRGG GGGBBBBB -> LSB
    // 24/32 bits: MSB <- [AAAAAAAA] RRRRRRRR GGGGGGGG BBBBBBBB -> LSB
    const int num_bits = (info_header_.bit_count == 16) ? 5 : 8;
    for (int i = 0; i <= 2; ++i) {
      bit_masks_[i] = ((uint32_t{1} << (num_bits * (3 - i))) - 1) ^
                      ((uint32_t{1} << (num_bits * (2 - i))) - 1);
    }
  } else if (!HasRGBMasksInHeader()) {
    // For HasRGBMasksInHeader() bitmaps, this was already done when we read the
    // info header.

    // Fail if we don't have enough file space for the bitmasks.
    const size_t header_end = header_offset_ + info_header_.size;
    const bool read_alpha = info_header_.compression == ALPHABITFIELDS;
    const size_t kBitmasksSize = read_alpha ? 16 : 12;
    const size_t bitmasks_end = header_end + kBitmasksSize;
    if ((bitmasks_end < header_end) ||
        (img_data_offset_ && (img_data_offset_ < bitmasks_end)))
      return parent_->SetFailed();

    // Read bitmasks.
    if ((data_->size() - decoded_offset_) < kBitmasksSize)
      return false;
    bit_masks_[0] = ReadUint32(0);
    bit_masks_[1] = ReadUint32(4);
    bit_masks_[2] = ReadUint32(8);
    if (read_alpha)
      bit_masks_[3] = ReadUint32(12);

    decoded_offset_ += kBitmasksSize;
  }

  // Alpha is a poorly-documented and inconsistently-used feature.
  //
  // BITMAPV3HEADER+ have an alpha bitmask in the info header.  Unlike the R/G/B
  // bitmasks, the MSDN docs don't indicate that it is only valid for the
  // BITFIELDS compression format, so we respect it at all times.
  //
  // Windows CE supports the ALPHABITFIELDS compression format, which is rare.
  // We assume any mask specified by this format is valid as well.
  //
  // To complicate things, Windows V3 BMPs, which lack a mask, can specify 32bpp
  // format, which to any sane reader would imply an 8-bit alpha channel -- and
  // for BMPs-in-ICOs, that's precisely what's intended to happen. There also
  // exist standalone BMPs in this format which clearly expect the alpha channel
  // to be respected. However, there are many other BMPs which, for example,
  // fill this channel with all 0s, yet clearly expect to not be displayed as a
  // fully-transparent rectangle.
  //
  // If these were the only two types of Windows V3, 32bpp BMPs in the wild,
  // we could distinguish between them by scanning the alpha channel in the
  // image, looking for nonzero values, and only enabling alpha if we found
  // some. (It turns out we have to do this anyway, because, crazily, there
  // are also Windows V4+ BMPs with an explicit, non-zero alpha mask, which
  // then zero-fill their alpha channels! See comments in
  // processNonRLEData().)
  //
  // Unfortunately there are also V3 BMPs -- indeed, probably more than the
  // number of 32bpp, V3 BMPs which intentionally use alpha -- which specify
  // 32bpp format, use nonzero (and non-255) alpha values, and yet expect to
  // be rendered fully-opaque. And other browsers do so.
  //
  // So it's impossible to display every BMP in the way its creators intended,
  // and we have to choose what to break. Given the paragraph above, we match
  // other browsers and ignore alpha in Windows V3 BMPs except inside ICO
  // files.
  if (!HasAlphaMaskInHeader() && (info_header_.compression != ALPHABITFIELDS)) {
    const bool use_mask = is_in_ico_ &&
                          (info_header_.compression != BITFIELDS) &&
                          (info_header_.bit_count == 32);
    bit_masks_[3] = use_mask ? uint32_t{0xff000000} : 0;
  }

  // Check masks and set shift and LUT address values.
  for (int i = 0; i < 4; ++i) {
    // Trim the mask to the allowed bit depth.  Some Windows V4+ BMPs
    // specify a bogus alpha channel in bits that don't exist in the pixel
    // data (for example, bits 25-31 in a 24-bit RGB format).
    if (info_header_.bit_count < 32) {
      bit_masks_[i] &= ((uint32_t{1} << info_header_.bit_count) - 1);
    }

    // For empty masks (common on the alpha channel, especially after the
    // trimming above), quickly clear the shift and LUT address and
    // continue, to avoid an infinite loop in the counting code below.
    uint32_t temp_mask = bit_masks_[i];
    if (!temp_mask) {
      bit_shifts_right_[i] = 0;
      lookup_table_addresses_[i] = nullptr;
      continue;
    }

    // Make sure bitmask does not overlap any other bitmasks.
    for (int j = 0; j < i; ++j) {
      if (temp_mask & bit_masks_[j])
        return parent_->SetFailed();
    }

    // Count offset into pixel data.
    for (bit_shifts_right_[i] = 0; !(temp_mask & 1); temp_mask >>= 1)
      ++bit_shifts_right_[i];

    // Count size of mask.
    size_t num_bits = 0;
    for (; temp_mask & 1; temp_mask >>= 1)
      ++num_bits;

    // Make sure bitmask is contiguous.
    if (temp_mask)
      return parent_->SetFailed();

    // Since RGBABuffer tops out at 8 bits per channel, adjust the shift
    // amounts to use the most significant 8 bits of the channel.
    if (num_bits >= 8) {
      bit_shifts_right_[i] += (num_bits - 8);
      num_bits = 0;
    }

    // Calculate LUT address.
    lookup_table_addresses_[i] =
        num_bits ? (nBitTo8BitlookupTable + (1 << num_bits) - 2) : nullptr;
  }

  // We've now decoded all the non-image data we care about.  Skip anything
  // else before the actual raster data.
  if (img_data_offset_)
    decoded_offset_ = img_data_offset_;
  need_to_process_bitmasks_ = false;
  return true;
}

bool BMPImageReader::ProcessColorTable() {
  // On non-OS/2 1.x, an extra padding byte is present, which we need to skip.
  const size_t bytes_per_color = is_os21x_ ? 3 : 4;

  const size_t header_end = header_offset_ + info_header_.size;
  size_t colors_in_palette = info_header_.clr_used;
  size_t table_size_in_bytes = colors_in_palette * bytes_per_color;
  const size_t table_end = header_end + table_size_in_bytes;
  if (table_end < header_end)
    return parent_->SetFailed();

  // Some BMPs don't contain a complete palette.  Avoid reading off the end.
  if (img_data_offset_ && (img_data_offset_ < table_end)) {
    colors_in_palette = (img_data_offset_ - header_end) / bytes_per_color;
    table_size_in_bytes = colors_in_palette * bytes_per_color;
  }

  // Read color table.
  if ((decoded_offset_ > data_->size()) ||
      ((data_->size() - decoded_offset_) < table_size_in_bytes))
    return false;
  color_table_.resize(info_header_.clr_used);

  for (size_t i = 0; i < colors_in_palette; ++i) {
    color_table_[i].rgb_blue = ReadUint8(0);
    color_table_[i].rgb_green = ReadUint8(1);
    color_table_[i].rgb_red = ReadUint8(2);
    decoded_offset_ += bytes_per_color;
  }
  // Explicitly zero any colors past the end of a truncated palette.
  for (size_t i = colors_in_palette; i < info_header_.clr_used; ++i) {
    color_table_[i].rgb_blue = 0;
    color_table_[i].rgb_green = 0;
    color_table_[i].rgb_red = 0;
  }

  // We've now decoded all the non-image data we care about.  Skip anything
  // else before the actual raster data.
  if (img_data_offset_)
    decoded_offset_ = img_data_offset_;
  need_to_process_color_table_ = false;
  return true;
}

bool BMPImageReader::InitFrame() {
  if (!buffer_->AllocatePixelData(parent_->Size().Width(),
                                  parent_->Size().Height(),
                                  parent_->ColorSpaceForSkImages()))
    return parent_->SetFailed();  // Unable to allocate.

  buffer_->ZeroFillPixelData();
  buffer_->SetStatus(ImageFrame::kFramePartial);
  // SetSize() calls EraseARGB(), which resets the alpha flag, so we force it
  // back to false here.  We'll set it to true later in all cases where these 0s
  // could actually show through.
  buffer_->SetHasAlpha(false);

  // For BMPs, the frame always fills the entire image.
  buffer_->SetOriginalFrameRect(IntRect(IntPoint(), parent_->Size()));

  if (!is_top_down_)
    coord_.SetY(parent_->Size().Height() - 1);
  return true;
}

bool BMPImageReader::DecodePixelData(bool non_rle) {
  const IntPoint coord(coord_);
  const ProcessingResult result =
      non_rle ? ProcessNonRLEData(false, 0) : ProcessRLEData();
  if (coord_ != coord)
    buffer_->SetPixelsChanged(true);
  return (result == kFailure) ? parent_->SetFailed() : (result == kSuccess);
}

BMPImageReader::ProcessingResult BMPImageReader::ProcessRLEData() {
  if (decoded_offset_ > data_->size())
    return kInsufficientData;

  // RLE decoding is poorly specified.  Two main problems:
  // (1) Are EOL markers necessary?  What happens when we have too many
  //     pixels for one row?
  //     http://www.fileformat.info/format/bmp/egff.htm says extra pixels
  //     should wrap to the next line.  Real BMPs I've encountered seem to
  //     instead expect extra pixels to be ignored until the EOL marker is
  //     seen, although this has only happened in a few cases and I suspect
  //     those BMPs may be invalid.  So we only change lines on EOL (or Delta
  //     with dy > 0), and fail in most cases when pixels extend past the end
  //     of the line.
  // (2) When Delta, EOL, or EOF are seen, what happens to the "skipped"
  //     pixels?
  //     http://www.daubnet.com/formats/BMP.html says these should be filled
  //     with color 0.  However, the "do nothing" and "don't care" comments
  //     of other references suggest leaving these alone, i.e. letting them
  //     be transparent to the background behind the image.  This seems to
  //     match how MSPAINT treats BMPs, so we do that.  Note that when we
  //     actually skip pixels for a case like this, we need to note on the
  //     framebuffer that we have alpha.

  // Impossible to decode row-at-a-time, so just do things as a stream of
  // bytes.
  while (true) {
    // Every entry takes at least two bytes; bail if there isn't enough
    // data.
    if ((data_->size() - decoded_offset_) < 2)
      return kInsufficientData;

    // For every entry except EOF, we'd better not have reached the end of
    // the image.
    const uint8_t count = ReadUint8(0);
    const uint8_t code = ReadUint8(1);
    if ((count || (code != 1)) && PastEndOfImage(0))
      return kFailure;

    // Decode.
    if (!count) {
      switch (code) {
        case 0:  // Magic token: EOL
          // Skip any remaining pixels in this row.
          if (coord_.X() < parent_->Size().Width())
            buffer_->SetHasAlpha(true);
          ColorCorrectCurrentRow();
          MoveBufferToNextRow();

          decoded_offset_ += 2;
          break;

        case 1:  // Magic token: EOF
          // Skip any remaining pixels in the image.
          if ((coord_.X() < parent_->Size().Width()) ||
              (is_top_down_ ? (coord_.Y() < (parent_->Size().Height() - 1))
                            : (coord_.Y() > 0)))
            buffer_->SetHasAlpha(true);
          ColorCorrectCurrentRow();
          // There's no need to move |coord_| here to trigger the caller
          // to call SetPixelsChanged().  If the only thing that's changed
          // is the alpha state, that will be properly written into the
          // underlying SkBitmap when we mark the frame complete.
          return kSuccess;

        case 2: {  // Magic token: Delta
          // The next two bytes specify dx and dy.  Bail if there isn't
          // enough data.
          if ((data_->size() - decoded_offset_) < 4)
            return kInsufficientData;

          // Fail if this takes us past the end of the desired row or
          // past the end of the image.
          const uint8_t dx = ReadUint8(2);
          const uint8_t dy = ReadUint8(3);
          if (dx || dy) {
            buffer_->SetHasAlpha(true);
            if (dy)
              ColorCorrectCurrentRow();
          }
          if (((coord_.X() + dx) > parent_->Size().Width()) ||
              PastEndOfImage(dy))
            return kFailure;

          // Skip intervening pixels.
          coord_.Move(dx, is_top_down_ ? dy : -dy);

          decoded_offset_ += 4;
          break;
        }

        default: {  // Absolute mode
          // |code| pixels specified as in BI_RGB, zero-padded at the end
          // to a multiple of 16 bits.
          // Because ProcessNonRLEData() expects decoded_offset_ to
          // point to the beginning of the pixel data, bump it past
          // the escape bytes and then reset if decoding failed.
          decoded_offset_ += 2;
          const ProcessingResult result = ProcessNonRLEData(true, code);
          if (result != kSuccess) {
            decoded_offset_ -= 2;
            return result;
          }
          break;
        }
      }
    } else {  // Encoded mode
      // The following color data is repeated for |count| total pixels.
      // Strangely, some BMPs seem to specify excessively large counts
      // here; ignore pixels past the end of the row.
      const int end_x = std::min(coord_.X() + count, parent_->Size().Width());

      if (info_header_.compression == RLE24) {
        // Bail if there isn't enough data.
        if ((data_->size() - decoded_offset_) < 4)
          return kInsufficientData;

        // One BGR triple that we copy |count| times.
        FillRGBA(end_x, ReadUint8(3), ReadUint8(2), code, 0xff);
        decoded_offset_ += 4;
      } else {
        // RLE8 has one color index that gets repeated; RLE4 has two
        // color indexes in the upper and lower 4 bits of the byte,
        // which are alternated.
        size_t color_indexes[2] = {code, code};
        if (info_header_.compression == RLE4) {
          color_indexes[0] = (color_indexes[0] >> 4) & 0xf;
          color_indexes[1] &= 0xf;
        }
        for (int which = 0; coord_.X() < end_x;) {
          // Some images specify color values past the end of the
          // color table; set these pixels to black.
          if (color_indexes[which] < info_header_.clr_used)
            SetI(color_indexes[which]);
          else
            SetRGBA(0, 0, 0, 255);
          which = !which;
        }

        decoded_offset_ += 2;
      }
    }
  }
}

BMPImageReader::ProcessingResult BMPImageReader::ProcessNonRLEData(
    bool in_rle,
    int num_pixels) {
  if (decoded_offset_ > data_->size())
    return kInsufficientData;

  if (!in_rle)
    num_pixels = parent_->Size().Width();

  // Fail if we're being asked to decode more pixels than remain in the row.
  const int end_x = coord_.X() + num_pixels;
  if (end_x > parent_->Size().Width())
    return kFailure;

  // Determine how many bytes of data the requested number of pixels
  // requires.
  const size_t pixels_per_byte = 8 / info_header_.bit_count;
  const size_t bytes_per_pixel = info_header_.bit_count / 8;
  const size_t unpadded_num_bytes =
      (info_header_.bit_count < 16)
          ? ((num_pixels + pixels_per_byte - 1) / pixels_per_byte)
          : (num_pixels * bytes_per_pixel);
  // RLE runs are zero-padded at the end to a multiple of 16 bits.  Non-RLE
  // data is in rows and is zero-padded to a multiple of 32 bits.
  const size_t align_bits = in_rle ? 1 : 3;
  const size_t padded_num_bytes =
      (unpadded_num_bytes + align_bits) & ~align_bits;

  // Decode as many rows as we can.  (For RLE, where we only want to decode
  // one row, we've already checked that this condition is true.)
  while (!PastEndOfImage(0)) {
    // Bail if we don't have enough data for the desired number of pixels.
    if ((data_->size() - decoded_offset_) < padded_num_bytes)
      return kInsufficientData;

    if (info_header_.bit_count < 16) {
      // Paletted data.  Pixels are stored little-endian within bytes.
      // Decode pixels one byte at a time, left to right (so, starting at
      // the most significant bits in the byte).
      const uint8_t mask = (1 << info_header_.bit_count) - 1;
      for (size_t end_offset = decoded_offset_ + unpadded_num_bytes;
           decoded_offset_ < end_offset; ++decoded_offset_) {
        uint8_t pixel_data = ReadUint8(0);
        for (size_t pixel = 0;
             (pixel < pixels_per_byte) && (coord_.X() < end_x); ++pixel) {
          const size_t color_index =
              (pixel_data >> (8 - info_header_.bit_count)) & mask;
          if (decoding_and_mask_) {
            // There's no way to accurately represent an AND + XOR
            // operation as an RGBA image, so where the AND values
            // are 1, we simply set the framebuffer pixels to fully
            // transparent, on the assumption that most ICOs on the
            // web will not be doing a lot of inverting.
            if (color_index) {
              SetRGBA(0, 0, 0, 0);
              buffer_->SetHasAlpha(true);
            } else {
              coord_.Move(1, 0);
            }
          } else {
            // See comments near the end of ProcessRLEData().
            if (color_index < info_header_.clr_used)
              SetI(color_index);
            else
              SetRGBA(0, 0, 0, 255);
          }
          pixel_data <<= info_header_.bit_count;
        }
      }
    } else {
      // RGB data.  Decode pixels one at a time, left to right.
      for (; coord_.X() < end_x; decoded_offset_ += bytes_per_pixel) {
        const uint32_t pixel = ReadCurrentPixel(bytes_per_pixel);

        // Some BMPs specify an alpha channel but don't actually use it
        // (it contains all 0s).  To avoid displaying these images as
        // fully-transparent, decode as if images are fully opaque
        // until we actually see a non-zero alpha value; at that point,
        // reset any previously-decoded pixels to fully transparent and
        // continue decoding based on the real alpha channel values.
        // As an optimization, avoid calling SetHasAlpha(true) for
        // images where all alpha values are 255; opaque images are
        // faster to draw.
        int alpha = GetAlpha(pixel);
        if (!seen_non_zero_alpha_pixel_ && !alpha) {
          seen_zero_alpha_pixel_ = true;
          alpha = 255;
        } else {
          seen_non_zero_alpha_pixel_ = true;
          if (seen_zero_alpha_pixel_) {
            buffer_->ZeroFillPixelData();
            seen_zero_alpha_pixel_ = false;
          } else if (alpha != 255) {
            buffer_->SetHasAlpha(true);
          }
        }

        SetRGBA(GetComponent(pixel, 0), GetComponent(pixel, 1),
                GetComponent(pixel, 2), alpha);
      }
    }

    // Success, keep going.
    decoded_offset_ += (padded_num_bytes - unpadded_num_bytes);
    if (in_rle)
      return kSuccess;
    ColorCorrectCurrentRow();
    MoveBufferToNextRow();
  }

  // Finished decoding whole image.
  return kSuccess;
}

void BMPImageReader::MoveBufferToNextRow() {
  coord_.Move(-coord_.X(), is_top_down_ ? 1 : -1);
}

void BMPImageReader::ColorCorrectCurrentRow() {
  if (decoding_and_mask_)
    return;
  // Postprocess the image data according to the profile.
  const ColorProfileTransform* const transform = parent_->ColorTransform();
  if (!transform)
    return;
  ImageFrame::PixelData* const row = buffer_->GetAddr(0, coord_.Y());
  const skcms_PixelFormat fmt = XformColorFormat();
  const skcms_AlphaFormat alpha =
      (buffer_->HasAlpha() && buffer_->PremultiplyAlpha())
          ? skcms_AlphaFormat_PremulAsEncoded
          : skcms_AlphaFormat_Unpremul;
  const bool success =
      skcms_Transform(row, fmt, alpha, transform->SrcProfile(), row, fmt, alpha,
                      transform->DstProfile(), parent_->Size().Width());
  DCHECK(success);
  buffer_->SetPixelsChanged(true);
}

}  // namespace blink
