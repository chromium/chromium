/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

#include <memory>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "skia/ext/cicp.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/gif/gif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/webp/webp_image_decoder.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/private/SkExif.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

#if BUILDFLAG(ENABLE_AV1_DECODER)
#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/avif/crabbyavif_image_decoder.h"
#endif

namespace blink {

namespace {

cc::ImageType FileExtensionToImageType(String image_extension) {
  if (image_extension == "png") {
    return cc::ImageType::kPNG;
  }
  if (image_extension == "jpg") {
    return cc::ImageType::kJPEG;
  }
  if (image_extension == "webp") {
    return cc::ImageType::kWEBP;
  }
  if (image_extension == "gif") {
    return cc::ImageType::kGIF;
  }
  if (image_extension == "ico") {
    return cc::ImageType::kICO;
  }
  if (image_extension == "bmp") {
    return cc::ImageType::kBMP;
  }
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (image_extension == "avif") {
    return cc::ImageType::kAVIF;
  }
#endif
  return cc::ImageType::kInvalid;
}

wtf_size_t CalculateMaxDecodedBytes(
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    const SkISize& desired_size,
    size_t platform_max_decoded_bytes) {
  const wtf_size_t max_decoded_bytes =
      base::saturated_cast<wtf_size_t>(platform_max_decoded_bytes);
  if (desired_size.isEmpty()) {
    return max_decoded_bytes;
  }

  const wtf_size_t num_pixels = desired_size.width() * desired_size.height();
  if (high_bit_depth_decoding_option == ImageDecoder::kDefaultBitDepth) {
    return std::min(4 * num_pixels, max_decoded_bytes);
  }

  // ImageDecoder::kHighBitDepthToHalfFloat
  return std::min(8 * num_pixels, max_decoded_bytes);
}

// Compute the density corrected size based on |metadata| and the physical size
// of the associated image.
gfx::Size ExtractDensityCorrectedSize(const SkExif::Metadata& metadata,
                                      const gfx::Size& physical_size) {
  const unsigned kDefaultResolution = 72;
  const unsigned kResolutionUnitDpi = 2;

  gfx::SizeF resolution(metadata.fXResolution.value_or(0),
                        metadata.fYResolution.value_or(0));
  gfx::Size size(metadata.fPixelXDimension.value_or(0),
                 metadata.fPixelYDimension.value_or(0));
  if (metadata.fResolutionUnit != kResolutionUnitDpi || resolution.IsEmpty() ||
      size.IsEmpty()) {
    return physical_size;
  }

  // Division by zero is not possible since we check for empty resolution
  // earlier.
  gfx::SizeF size_from_resolution(
      physical_size.width() * kDefaultResolution / resolution.width(),
      physical_size.height() * kDefaultResolution / resolution.height());

  if (gfx::ToRoundedSize(size_from_resolution) == size) {
    return size;
  }

  return physical_size;
}

inline bool MatchesJPEGSignature(const char* contents) {
  return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

inline bool MatchesPNGSignature(const char* contents) {
  return !memcmp(contents, "\x89PNG\r\n\x1A\n", 8);
}

inline bool MatchesGIFSignature(const char* contents) {
  return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

inline bool MatchesWebPSignature(const char* contents) {
  return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}

inline bool MatchesICOSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

inline bool MatchesCURSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

inline bool MatchesBMPSignature(const char* contents) {
  return !memcmp(contents, "BM", 2) || !memcmp(contents, "BA", 2);
}

constexpr wtf_size_t kLongestSignatureLength = sizeof("RIFF????WEBPVP") - 1;

// static
String SniffMimeTypeInternal(scoped_refptr<SegmentReader> reader) {
  // At least kLongestSignatureLength bytes are needed to sniff the signature.
  if (reader->size() < kLongestSignatureLength) {
    return String();
  }

  // Access the first kLongestSignatureLength chars to sniff the signature.
  // (note: FastSharedBufferReader only makes a copy if the bytes are segmented)
  char buffer[kLongestSignatureLength];
  const FastSharedBufferReader fast_reader(reader);
  const char* contents =
      fast_reader.GetConsecutiveData(0, kLongestSignatureLength, buffer);

  if (MatchesJPEGSignature(contents)) {
    return "image/jpeg";
  }
  if (MatchesPNGSignature(contents)) {
    return "image/png";
  }
  if (MatchesGIFSignature(contents)) {
    return "image/gif";
  }
  if (MatchesWebPSignature(contents)) {
    return "image/webp";
  }
  if (MatchesICOSignature(contents) || MatchesCURSignature(contents)) {
    return "image/x-icon";
  }
  if (MatchesBMPSignature(contents)) {
    return "image/bmp";
  }
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kCrabbyAvif)
          ? CrabbyAVIFImageDecoder::MatchesAVIFSignature(fast_reader)
          : AVIFImageDecoder::MatchesAVIFSignature(fast_reader)) {
    return "image/avif";
  }
#endif

  return String();
}

// Checks to see if a mime type is an image type with lossy compression, whose
// size will be restricted via the 'lossy-images-max-bpp' document
// policy. (JPEG)
bool IsLossyImageMIMEType(const String& mime_type) {
  return EqualIgnoringASCIICase(mime_type, "image/jpeg") ||
         EqualIgnoringASCIICase(mime_type, "image/jpg") ||
         EqualIgnoringASCIICase(mime_type, "image/pjpeg");
}

// Checks to see if a mime type is an image type with lossless (or no)
// compression, whose size may be restricted via the
// 'lossless-images-max-bpp' document policy. (BMP, GIF, PNG, WEBP)
bool IsLosslessImageMIMEType(const String& mime_type) {
  return EqualIgnoringASCIICase(mime_type, "image/bmp") ||
         EqualIgnoringASCIICase(mime_type, "image/gif") ||
         EqualIgnoringASCIICase(mime_type, "image/png") ||
         EqualIgnoringASCIICase(mime_type, "image/webp") ||
         EqualIgnoringASCIICase(mime_type, "image/x-xbitmap") ||
         EqualIgnoringASCIICase(mime_type, "image/x-png");
}

}  // namespace

ImageDecoder::ImageDecoder(
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    cc::AuxImage aux_image,
    wtf_size_t max_decoded_bytes)
    : premultiply_alpha_(alpha_option == kAlphaPremultiplied),
      high_bit_depth_decoding_option_(high_bit_depth_decoding_option),
      color_behavior_(color_behavior),
      aux_image_(aux_image),
      max_decoded_bytes_(max_decoded_bytes),
      allow_decode_to_yuv_(false),
      purge_aggressively_(false) {}

ImageDecoder::~ImageDecoder() = default;

std::unique_ptr<ImageDecoder> ImageDecoder::Create(
    scoped_refptr<SegmentReader> data,
    bool data_complete,
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    cc::AuxImage aux_image,
    size_t platform_max_decoded_bytes,
    const SkISize& desired_size,
    AnimationOption animation_option) {
  auto type = SniffMimeTypeInternal(data);
  if (type.empty()) {
    return nullptr;
  }

  return CreateByMimeType(type, std::move(data), data_complete, alpha_option,
                          high_bit_depth_decoding_option, color_behavior,
                          aux_image, platform_max_decoded_bytes, desired_size,
                          animation_option);
}

std::unique_ptr<ImageDecoder> ImageDecoder::CreateByMimeType(
    String mime_type,
    scoped_refptr<SegmentReader> data,
    bool data_complete,
    AlphaOption alpha_option,
    HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    cc::AuxImage aux_image,
    size_t platform_max_decoded_bytes,
    const SkISize& desired_size,
    AnimationOption animation_option) {
  const wtf_size_t max_decoded_bytes = CalculateMaxDecodedBytes(
      high_bit_depth_decoding_option, desired_size, platform_max_decoded_bytes);

  // Note: The mime types below should match those supported by
  // MimeUtil::IsSupportedImageMimeType() (which forces lowercase).
  std::unique_ptr<ImageDecoder> decoder;
  mime_type = mime_type.LowerASCII();
  if (mime_type == "image/jpeg" || mime_type == "image/pjpeg" ||
      mime_type == "image/jpg") {
    decoder = std::make_unique<JPEGImageDecoder>(alpha_option, color_behavior,
                                                 aux_image, max_decoded_bytes);
  } else if (mime_type == "image/png" || mime_type == "image/x-png" ||
             mime_type == "image/apng") {
    decoder = std::make_unique<PNGImageDecoder>(
        alpha_option, high_bit_depth_decoding_option, color_behavior,
        max_decoded_bytes);
  } else if (mime_type == "image/gif") {
    decoder = std::make_unique<GIFImageDecoder>(alpha_option, color_behavior,
                                                max_decoded_bytes);
  } else if (mime_type == "image/webp") {
    decoder = std::make_unique<WEBPImageDecoder>(alpha_option, color_behavior,
                                                 max_decoded_bytes);
  } else if (mime_type == "image/x-icon" ||
             mime_type == "image/vnd.microsoft.icon") {
    decoder = std::make_unique<ICOImageDecoder>(alpha_option, color_behavior,
                                                max_decoded_bytes);
  } else if (mime_type == "image/bmp" || mime_type == "image/x-xbitmap") {
    decoder = std::make_unique<BMPImageDecoder>(alpha_option, color_behavior,
                                                max_decoded_bytes);
#if BUILDFLAG(ENABLE_AV1_DECODER)
  } else if (mime_type == "image/avif") {
    if (base::FeatureList::IsEnabled(blink::features::kCrabbyAvif)) {
      decoder = std::make_unique<CrabbyAVIFImageDecoder>(
          alpha_option, high_bit_depth_decoding_option, color_behavior,
          max_decoded_bytes, animation_option);
    } else {
      decoder = std::make_unique<AVIFImageDecoder>(
          alpha_option, high_bit_depth_decoding_option, color_behavior,
          max_decoded_bytes, animation_option);
    }
#endif
  }

  if (decoder) {
    decoder->SetData(std::move(data), data_complete);
  }

  return decoder;
}

bool ImageDecoder::IsAllDataReceived() const {
  return is_all_data_received_;
}

bool ImageDecoder::ImageIsHighBitDepth() {
  return false;
}

bool ImageDecoder::HasSufficientDataToSniffMimeType(const SharedBuffer& data) {
  // At least kLongestSignatureLength bytes are needed to sniff the signature.
  if (data.size() < kLongestSignatureLength) {
    return false;
  }

#if BUILDFLAG(ENABLE_AV1_DECODER)
  {
    // Check for an ISO BMFF File Type Box. Assume that 'largesize' is not used.
    // The first eight bytes would be a big-endian 32-bit unsigned integer
    // 'size' and a four-byte 'type'.
    struct {
      uint8_t size[4];  // unsigned int(32) size;
      char type[4];   // unsigned int(32) type = boxtype;
    } box;
    static_assert(sizeof(box) == 8, "");
    static_assert(8 <= kLongestSignatureLength, "");
    bool ok = data.GetBytes(&box, 8u);
    DCHECK(ok);
    if (base::span(box.type) == base::span({'f', 't', 'y', 'p'})) {
      // Returns whether we have received the File Type Box in its entirety.
      return base::U32FromBigEndian(box.size) <= data.size();
    }
  }
#endif

  return true;
}

// static
String ImageDecoder::SniffMimeType(scoped_refptr<SharedBuffer> image_data) {
  return SniffMimeTypeInternal(
      SegmentReader::CreateFromSharedBuffer(std::move(image_data)));
}

// static
ImageDecoder::CompressionFormat ImageDecoder::GetCompressionFormat(
    scoped_refptr<SharedBuffer> image_data,
    String mime_type) {
  // Attempt to sniff the image content to determine the true MIME type of the
  // image, and fall back on the provided MIME type if this is not possible.
  //
  // Note that if the type cannot be sniffed AND the provided type is incorrect
  // (for example, due to a misconfigured web server), then it is possible that
  // the wrong compression format will be returned. However, this case should be
  // exceedingly rare.
  if (image_data && HasSufficientDataToSniffMimeType(*image_data.get())) {
    mime_type = SniffMimeType(image_data);
  }
  if (!mime_type) {
    return kUndefinedFormat;
  }

  // Attempt to sniff whether a WebP image is using a lossy or lossless
  // compression algorithm. Note: Will return kWebPAnimationFormat in the case
  // of an animated WebP image.
  size_t available_data = image_data ? image_data->size() : 0;
  if (EqualIgnoringASCIICase(mime_type, "image/webp") && available_data >= 16) {
    // Attempt to sniff only 8 bytes (the second half of the first 16). This
    // will be sufficient to determine lossy vs. lossless in most WebP images
    // (all but the extended format).
    const FastSharedBufferReader fast_reader(
        SegmentReader::CreateFromSharedBuffer(image_data));
    char buffer[8];
    const unsigned char* contents = reinterpret_cast<const unsigned char*>(
        fast_reader.GetConsecutiveData(8, 8, buffer));
    if (!memcmp(contents, "WEBPVP8 ", 8)) {
      // Simple lossy WebP format.
      return kLossyFormat;
    }
    if (!memcmp(contents, "WEBPVP8L", 8)) {
      // Simple Lossless WebP format.
      return kLosslessFormat;
    }
    if (!memcmp(contents, "WEBPVP8X", 8)) {
      // Extended WebP format; more content will need to be sniffed to make a
      // determination.
      auto long_buffer = base::HeapArray<char>::Uninit(available_data);
      contents =
          reinterpret_cast<const unsigned char*>(fast_reader.GetConsecutiveData(
              0, available_data, long_buffer.data()));
      WebPBitstreamFeatures webp_features{};
      VP8StatusCode status =
          WebPGetFeatures(contents, available_data, &webp_features);
      // It is possible that there is not have enough image data available to
      // make a determination.
      if (status == VP8_STATUS_OK) {
        DCHECK_LT(webp_features.format,
                  CompressionFormat::kWebPAnimationFormat);
        return webp_features.has_animation
                   ? CompressionFormat::kWebPAnimationFormat
                   : static_cast<CompressionFormat>(webp_features.format);
      } else if (status != VP8_STATUS_NOT_ENOUGH_DATA) {
        return kUndefinedFormat;
      }
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

#if BUILDFLAG(ENABLE_AV1_DECODER)
  // Attempt to sniff whether an AVIF image is using a lossy or lossless
  // compression algorithm.
  // TODO(wtc): Implement this. Figure out whether to return kUndefinedFormat or
  // a new kAVIFAnimationFormat in the case of an animated AVIF image.
  if (EqualIgnoringASCIICase(mime_type, "image/avif")) {
    return kLossyFormat;
  }
#endif

  if (IsLossyImageMIMEType(mime_type)) {
    return kLossyFormat;
  }
  if (IsLosslessImageMIMEType(mime_type)) {
    return kLosslessFormat;
  }

  return kUndefinedFormat;
}

bool ImageDecoder::IsSizeAvailable() {
  if (failed_) {
    return false;
  }
  if (!size_available_) {
    DecodeSize();
  }

  if (!IsDecodedSizeAvailable()) {
    return false;
  }

#if BUILDFLAG(IS_FUCHSIA)
  unsigned decoded_bytes_per_pixel = 4;
  if (ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
    decoded_bytes_per_pixel = 8;
  }

  const gfx::Size size = DecodedSize();
  const wtf_size_t decoded_size_bytes =
      size.width() * size.height() * decoded_bytes_per_pixel;
  if (decoded_size_bytes > max_decoded_bytes_) {
    LOG(WARNING) << "Blocked decode of oversized image: " << size.width() << "x"
                 << size.height();
    return SetFailed();
  }
#endif

  return true;
}

gfx::Size ImageDecoder::Size() const {
  return size_;
}

Vector<SkISize> ImageDecoder::GetSupportedDecodeSizes() const {
  return {};
}

bool ImageDecoder::GetGainmapInfoAndData(
    SkGainmapInfo& out_gainmap_info,
    scoped_refptr<SegmentReader>& out_gainmap_data) const {
  return false;
}

gfx::Size ImageDecoder::DecodedSize() const {
  return Size();
}

cc::YUVSubsampling ImageDecoder::GetYUVSubsampling() const {
  return cc::YUVSubsampling::kUnknown;
}

gfx::Size ImageDecoder::DecodedYUVSize(cc::YUVIndex) const {
  NOTREACHED_IN_MIGRATION();
  return gfx::Size();
}

wtf_size_t ImageDecoder::DecodedYUVWidthBytes(cc::YUVIndex) const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

SkYUVColorSpace ImageDecoder::GetYUVColorSpace() const {
  NOTREACHED_IN_MIGRATION();
  return SkYUVColorSpace::kIdentity_SkYUVColorSpace;
}

uint8_t ImageDecoder::GetYUVBitDepth() const {
  return 8;
}

std::optional<gfx::HDRMetadata> ImageDecoder::GetHDRMetadata() const {
  return std::nullopt;
}

gfx::Size ImageDecoder::FrameSizeAtIndex(wtf_size_t) const {
  return Size();
}

cc::ImageHeaderMetadata ImageDecoder::MakeMetadataForDecodeAcceleration()
    const {
  DCHECK(IsDecodedSizeAvailable());
  cc::ImageHeaderMetadata image_metadata{};
  image_metadata.image_type = FileExtensionToImageType(FilenameExtension());
  image_metadata.yuv_subsampling = GetYUVSubsampling();
  image_metadata.hdr_metadata = GetHDRMetadata();
  image_metadata.image_size = size_;
  image_metadata.has_embedded_color_profile = HasEmbeddedColorProfile();
  return image_metadata;
}

bool ImageDecoder::SetSize(unsigned width, unsigned height) {
  unsigned decoded_bytes_per_pixel = 4;
  if (ImageIsHighBitDepth() &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat) {
    decoded_bytes_per_pixel = 8;
  }
  if (SizeCalculationMayOverflow(width, height, decoded_bytes_per_pixel)) {
    return SetFailed();
  }

  size_ = gfx::Size(width, height);
  size_available_ = true;
  return true;
}

wtf_size_t ImageDecoder::FrameCount() {
  const wtf_size_t old_size = frame_buffer_cache_.size();
  const wtf_size_t new_size = DecodeFrameCount();
  if (old_size != new_size) {
    frame_buffer_cache_.resize(new_size);
    for (wtf_size_t i = old_size; i < new_size; ++i) {
      frame_buffer_cache_[i].SetPremultiplyAlpha(premultiply_alpha_);
      InitializeNewFrame(i);
    }
  }
  return new_size;
}

int ImageDecoder::RepetitionCount() const {
  return kAnimationNone;
}

ImageFrame* ImageDecoder::DecodeFrameBufferAtIndex(wtf_size_t index) {
  TRACE_EVENT0("blink", "ImageDecoder::DecodeFrameBufferAtIndex");

  if (index >= FrameCount()) {
    return nullptr;
  }
  ImageFrame* frame = &frame_buffer_cache_[index];
  if (frame->GetStatus() != ImageFrame::kFrameComplete) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Decode Image",
                 "imageType", FilenameExtension().Ascii());
    Decode(index);
  }

  frame->NotifyBitmapIfPixelsChanged();
  return frame;
}

bool ImageDecoder::FrameHasAlphaAtIndex(wtf_size_t index) const {
  return !FrameIsReceivedAtIndex(index) ||
         frame_buffer_cache_[index].HasAlpha();
}

bool ImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  // Animated images override this method to return the status based on the data
  // received for the queried frame.
  return IsAllDataReceived();
}

bool ImageDecoder::FrameIsDecodedAtIndex(wtf_size_t index) const {
  return index < frame_buffer_cache_.size() &&
         frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete;
}

std::optional<base::TimeDelta> ImageDecoder::FrameTimestampAtIndex(
    wtf_size_t) const {
  return std::nullopt;
}

base::TimeDelta ImageDecoder::FrameDurationAtIndex(wtf_size_t) const {
  return base::TimeDelta();
}

wtf_size_t ImageDecoder::FrameBytesAtIndex(wtf_size_t index) const {
  if (index >= frame_buffer_cache_.size() ||
      frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameEmpty) {
    return 0;
  }

  wtf_size_t decoded_bytes_per_pixel = 4;
  if (frame_buffer_cache_[index].GetPixelFormat() ==
      ImageFrame::PixelFormat::kRGBA_F16) {
    decoded_bytes_per_pixel = 8;
  }
  gfx::Size size = FrameSizeAtIndex(index);
  base::CheckedNumeric<wtf_size_t> area = size.width();
  area *= size.height();
  area *= decoded_bytes_per_pixel;
  return area.ValueOrDie();
}

bool ImageDecoder::SetFailed() {
  failed_ = true;
  return false;
}

wtf_size_t ImageDecoder::ClearCacheExceptFrame(wtf_size_t clear_except_frame) {
  // Don't clear if there are no frames or only one frame.
  if (frame_buffer_cache_.size() <= 1) {
    return 0;
  }

  // We expect that after this call, we'll be asked to decode frames after this
  // one. So we want to avoid clearing frames such that those requests would
  // force re-decoding from the beginning of the image. There are two cases in
  // which preserving |clear_except_frame| is not enough to avoid that:
  //
  // 1. |clear_except_frame| is not yet sufficiently decoded to decode
  //    subsequent frames. We need the previous frame to sufficiently decode
  //    this frame.
  // 2. The disposal method of |clear_except_frame| is DisposeOverwritePrevious.
  //    In that case, we need to keep the required previous frame in the cache
  //    to prevent re-decoding that frame when |clear_except_frame| is disposed.
  //
  // If either 1 or 2 is true, store the required previous frame in
  // |clear_except_frame2| so it won't be cleared.
  wtf_size_t clear_except_frame2 = kNotFound;
  if (clear_except_frame < frame_buffer_cache_.size()) {
    const ImageFrame& frame = frame_buffer_cache_[clear_except_frame];
    if (!FrameStatusSufficientForSuccessors(clear_except_frame) ||
        frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious) {
      clear_except_frame2 = frame.RequiredPreviousFrameIndex();
    }
  }

  // Now |clear_except_frame2| indicates the frame that |clear_except_frame|
  // depends on, as described above. But if decoding is skipping forward past
  // intermediate frames, this frame may be insufficiently decoded. So we need
  // to keep traversing back through the required previous frames until we find
  // the nearest ancestor that is sufficiently decoded. Preserving that will
  // minimize the amount of future decoding needed.
  while (clear_except_frame2 < frame_buffer_cache_.size() &&
         !FrameStatusSufficientForSuccessors(clear_except_frame2)) {
    clear_except_frame2 =
        frame_buffer_cache_[clear_except_frame2].RequiredPreviousFrameIndex();
  }

  return ClearCacheExceptTwoFrames(clear_except_frame, clear_except_frame2);
}

bool ImageDecoder::HotSpot(gfx::Point&) const {
  return false;
}

void ImageDecoder::SetMemoryAllocator(SkBitmap::Allocator* allocator) {
  // This currently doesn't work for images with multiple frames.
  // Some animated image formats require extra guarantees:
  // 1. The memory is cheaply readable, which isn't true for GPU memory, and
  // 2. The memory's lifetime will persist long enough to allow reading past
  //   frames, which isn't true for discardable memory.
  // Not all animated image formats share these requirements. Blocking
  // all animated formats is overly aggressive. If a need arises for an
  // external memory allocator for animated images, this should be changed.
  if (frame_buffer_cache_.empty()) {
    // Ensure that InitializeNewFrame is called, after parsing if
    // necessary.
    if (!FrameCount()) {
      return;
    }
  }

  frame_buffer_cache_[0].SetMemoryAllocator(allocator);
}

void ImageDecoder::DecodeToYUV() {
  NOTREACHED_IN_MIGRATION();
}

bool ImageDecoder::ImageHasBothStillAndAnimatedSubImages() const {
  return false;
}

wtf_size_t ImageDecoder::ClearCacheExceptTwoFrames(
    wtf_size_t clear_except_frame1,
    wtf_size_t clear_except_frame2) {
  wtf_size_t frame_bytes_cleared = 0;
  for (wtf_size_t i = 0; i < frame_buffer_cache_.size(); ++i) {
    if (frame_buffer_cache_[i].GetStatus() != ImageFrame::kFrameEmpty &&
        i != clear_except_frame1 && i != clear_except_frame2) {
      frame_bytes_cleared += FrameBytesAtIndex(i);
      ClearFrameBuffer(i);
    }
  }
  return frame_bytes_cleared;
}

void ImageDecoder::ClearFrameBuffer(wtf_size_t frame_index) {
  frame_buffer_cache_[frame_index].ClearPixelData();
}

wtf_size_t ImageDecoder::DecodeFrameCount() {
  return 1;
}

Vector<wtf_size_t> ImageDecoder::FindFramesToDecode(wtf_size_t index) const {
  DCHECK_LT(index, frame_buffer_cache_.size());

  Vector<wtf_size_t> frames_to_decode;
  do {
    frames_to_decode.push_back(index);
    index = frame_buffer_cache_[index].RequiredPreviousFrameIndex();
  } while (index != kNotFound && frame_buffer_cache_[index].GetStatus() !=
                                     ImageFrame::kFrameComplete);
  return frames_to_decode;
}

bool ImageDecoder::PostDecodeProcessing(wtf_size_t index) {
  DCHECK(index < frame_buffer_cache_.size());

  if (frame_buffer_cache_[index].GetStatus() != ImageFrame::kFrameComplete) {
    return false;
  }

  if (purge_aggressively_) {
    ClearCacheExceptFrame(index);
  }

  return true;
}

void ImageDecoder::CorrectAlphaWhenFrameBufferSawNoAlpha(wtf_size_t index) {
  DCHECK(index < frame_buffer_cache_.size());
  ImageFrame& buffer = frame_buffer_cache_[index];

  // When this frame spans the entire image rect we can SetHasAlpha to false,
  // since there are logically no transparent pixels outside of the frame rect.
  if (buffer.OriginalFrameRect().Contains(gfx::Rect(Size()))) {
    buffer.SetHasAlpha(false);
    buffer.SetRequiredPreviousFrameIndex(kNotFound);
  } else if (buffer.RequiredPreviousFrameIndex() != kNotFound) {
    // When the frame rect does not span the entire image rect, and it does
    // *not* have a required previous frame, the pixels outside of the frame
    // rect will be fully transparent, so we shoudn't SetHasAlpha to false.
    //
    // It is a tricky case when the frame does have a required previous frame.
    // The frame does not have alpha only if everywhere outside its rect
    // doesn't have alpha.  To know whether this is true, we check the start
    // state of the frame -- if it doesn't have alpha, we're safe.
    //
    // We first check that the required previous frame does not have
    // DisposeOverWritePrevious as its disposal method - this should never
    // happen, since the required frame should in that case be the required
    // frame of this frame's required frame.
    //
    // If |prev_buffer| is DisposeNotSpecified or DisposeKeep, |buffer| has no
    // alpha if |prev_buffer| had no alpha. Since InitFrameBuffer() already
    // copied the alpha state, there's nothing to do here.
    //
    // The only remaining case is a DisposeOverwriteBgcolor frame.  If
    // it had no alpha, and its rect is contained in the current frame's
    // rect, we know the current frame has no alpha.
    //
    // For DisposeNotSpecified, DisposeKeep and DisposeOverwriteBgcolor there
    // is one situation that is not taken into account - when |prev_buffer|
    // *does* have alpha, but only in the frame rect of |buffer|, we can still
    // say that this frame has no alpha. However, to determine this, we
    // potentially need to analyze all image pixels of |prev_buffer|, which is
    // too computationally expensive.
    const ImageFrame* prev_buffer =
        &frame_buffer_cache_[buffer.RequiredPreviousFrameIndex()];
    DCHECK(prev_buffer->GetDisposalMethod() !=
           ImageFrame::kDisposeOverwritePrevious);

    if ((prev_buffer->GetDisposalMethod() ==
         ImageFrame::kDisposeOverwriteBgcolor) &&
        !prev_buffer->HasAlpha() &&
        buffer.OriginalFrameRect().Contains(prev_buffer->OriginalFrameRect())) {
      buffer.SetHasAlpha(false);
    }
  }
}

bool ImageDecoder::InitFrameBuffer(wtf_size_t frame_index) {
  DCHECK(frame_index < frame_buffer_cache_.size());

  ImageFrame* const buffer = &frame_buffer_cache_[frame_index];

  // If the frame is already initialized, return true.
  if (buffer->GetStatus() != ImageFrame::kFrameEmpty) {
    return true;
  }

  wtf_size_t required_previous_frame_index =
      buffer->RequiredPreviousFrameIndex();
  if (required_previous_frame_index == kNotFound) {
    // This frame doesn't rely on any previous data.
    if (!buffer->AllocatePixelData(Size().width(), Size().height(),
                                   ColorSpaceForSkImages())) {
      return false;
    }
    buffer->ZeroFillPixelData();
  } else {
    ImageFrame* const prev_buffer =
        &frame_buffer_cache_[required_previous_frame_index];
    DCHECK(prev_buffer->GetStatus() == ImageFrame::kFrameComplete);

    // We try to reuse |prev_buffer| as starting state to avoid copying.
    // If CanReusePreviousFrameBuffer returns false, we must copy the data since
    // |prev_buffer| is necessary to decode this or later frames. In that case,
    // copy the data instead.
    if ((!CanReusePreviousFrameBuffer(frame_index) ||
         !buffer->TakeBitmapDataIfWritable(prev_buffer)) &&
        !buffer->CopyBitmapData(*prev_buffer)) {
      return false;
    }

    if (prev_buffer->GetDisposalMethod() ==
        ImageFrame::kDisposeOverwriteBgcolor) {
      // We want to clear the previous frame to transparent, without
      // affecting pixels in the image outside of the frame.
      const gfx::Rect& prev_rect = prev_buffer->OriginalFrameRect();
      DCHECK(!prev_rect.Contains(gfx::Rect(Size())));
      buffer->ZeroFillFrameRect(prev_rect);
    }
  }

  DCHECK_EQ(high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat &&
                ImageIsHighBitDepth(),
            buffer->GetPixelFormat() == ImageFrame::kRGBA_F16);

  OnInitFrameBuffer(frame_index);

  // Update our status to be partially complete.
  buffer->SetStatus(ImageFrame::kFramePartial);

  return true;
}

void ImageDecoder::UpdateAggressivePurging(wtf_size_t index) {
  if (purge_aggressively_) {
    return;
  }

  // We don't want to cache so much that we cause a memory issue.
  //
  // If we used a LRU cache we would fill it and then on next animation loop
  // we would need to decode all the frames again -- the LRU would give no
  // benefit and would consume more memory.
  // So instead, simply purge unused frames if caching all of the frames of
  // the image would use more memory than the image decoder is allowed
  // (|max_decoded_bytes|) or would overflow 32 bits..
  //
  // As we decode we will learn the total number of frames, and thus total
  // possible image memory used.

  wtf_size_t decoded_bytes_per_pixel = 4;

  if (frame_buffer_cache_.size() && frame_buffer_cache_[0].GetPixelFormat() ==
                                        ImageFrame::PixelFormat::kRGBA_F16) {
    decoded_bytes_per_pixel = 8;
  }
  const uint64_t frame_memory_usage =
      DecodedSize().Area64() * decoded_bytes_per_pixel;

  // This condition never fails in the current code. Our existing image decoders
  // parse for the image size and SetFailed() if that size overflows
  DCHECK_EQ(frame_memory_usage / decoded_bytes_per_pixel,
            DecodedSize().Area64());

  const uint64_t total_memory_usage = frame_memory_usage * index;
  if (total_memory_usage / frame_memory_usage != index) {  // overflow occurred
    purge_aggressively_ = true;
    return;
  }

  if (total_memory_usage > max_decoded_bytes_) {
    purge_aggressively_ = true;
  }
}

bool ImageDecoder::FrameStatusSufficientForSuccessors(wtf_size_t index) {
  DCHECK(index < frame_buffer_cache_.size());
  ImageFrame::Status frame_status = frame_buffer_cache_[index].GetStatus();
  return frame_status == ImageFrame::kFramePartial ||
         frame_status == ImageFrame::kFrameComplete;
}

wtf_size_t ImageDecoder::FindRequiredPreviousFrame(wtf_size_t frame_index,
                                                   bool frame_rect_is_opaque) {
  DCHECK_LT(frame_index, frame_buffer_cache_.size());
  if (!frame_index) {
    // The first frame doesn't rely on any previous data.
    return kNotFound;
  }

  const ImageFrame* curr_buffer = &frame_buffer_cache_[frame_index];
  if ((frame_rect_is_opaque ||
       curr_buffer->GetAlphaBlendSource() == ImageFrame::kBlendAtopBgcolor) &&
      curr_buffer->OriginalFrameRect().Contains(gfx::Rect(Size()))) {
    return kNotFound;
  }

  // The starting state for this frame depends on the previous frame's
  // disposal method.
  wtf_size_t prev_frame = frame_index - 1;
  const ImageFrame* prev_buffer = &frame_buffer_cache_[prev_frame];

  // Frames that use the DisposeOverwritePrevious method are effectively
  // no-ops in terms of changing the starting state of a frame compared to
  // the starting state of the previous frame, so skip over them.
  while (prev_buffer->GetDisposalMethod() ==
         ImageFrame::kDisposeOverwritePrevious) {
    if (prev_frame == 0) {
      return kNotFound;
    }
    prev_frame--;
    prev_buffer = &frame_buffer_cache_[prev_frame];
  }

  switch (prev_buffer->GetDisposalMethod()) {
    case ImageFrame::kDisposeNotSpecified:
    case ImageFrame::kDisposeKeep:
      // |prev_frame| will be used as the starting state for this frame.
      // FIXME: Be even smarter by checking the frame sizes and/or
      // alpha-containing regions.
      return prev_frame;
    case ImageFrame::kDisposeOverwriteBgcolor:
      // If the previous frame fills the whole image, then the current frame
      // can be decoded alone. Likewise, if the previous frame could be
      // decoded without reference to any prior frame, the starting state for
      // this frame is a blank frame, so it can again be decoded alone.
      // Otherwise, the previous frame contributes to this frame.
      return (prev_buffer->OriginalFrameRect().Contains(gfx::Rect(Size())) ||
              (prev_buffer->RequiredPreviousFrameIndex() == kNotFound))
                 ? kNotFound
                 : prev_frame;
    case ImageFrame::kDisposeOverwritePrevious:
    default:
      NOTREACHED_IN_MIGRATION();
      return kNotFound;
  }
}

void ImageDecoder::ApplyExifMetadata(const SkData* exif_data,
                                     const gfx::Size& physical_size) {
  DCHECK(IsDecodedSizeAvailable());
  SkExif::Metadata metadata;
  SkExif::Parse(metadata, exif_data);

  orientation_ = static_cast<ImageOrientationEnum>(
      metadata.fOrigin.value_or(kTopLeft_SkEncodedOrigin));
  density_corrected_size_ =
      ExtractDensityCorrectedSize(metadata, physical_size);
}

ImagePlanes::ImagePlanes() {
  color_type_ = kUnknown_SkColorType;
  for (int i = 0; i < cc::kNumYUVPlanes; ++i) {
    planes_[i] = nullptr;
    row_bytes_[i] = 0;
  }
}

ImagePlanes::ImagePlanes(void* planes[cc::kNumYUVPlanes],
                         const wtf_size_t row_bytes[cc::kNumYUVPlanes],
                         SkColorType color_type)
    : color_type_(color_type) {
  for (int i = 0; i < cc::kNumYUVPlanes; ++i) {
    planes_[i] = planes[i];
    row_bytes_[i] = row_bytes[i];
  }
}

void* ImagePlanes::Plane(cc::YUVIndex index) {
  return planes_[static_cast<wtf_size_t>(index)];
}

wtf_size_t ImagePlanes::RowBytes(cc::YUVIndex index) const {
  return row_bytes_[static_cast<wtf_size_t>(index)];
}

ColorProfile::ColorProfile(const skcms_ICCProfile& profile,
                           base::HeapArray<uint8_t> buffer)
    : profile_(profile), buffer_(std::move(buffer)) {}

ColorProfile::~ColorProfile() = default;

std::unique_ptr<ColorProfile> ColorProfile::Create(
    base::span<const uint8_t> buffer) {
  // After skcms_Parse, profile will have pointers into the passed buffer,
  // so we need to copy first, then parse.
  auto owned_buffer = base::HeapArray<uint8_t>::CopiedFrom(buffer);
  skcms_ICCProfile profile;
  if (skcms_Parse(owned_buffer.data(), owned_buffer.size(), &profile)) {
    return std::make_unique<ColorProfile>(profile, std::move(owned_buffer));
  }
  return nullptr;
}

ColorProfileTransform::ColorProfileTransform(
    const skcms_ICCProfile* src_profile,
    const skcms_ICCProfile* dst_profile) {
  DCHECK(src_profile);
  DCHECK(dst_profile);
  src_profile_ = src_profile;
  dst_profile_ = *dst_profile;
}

const skcms_ICCProfile* ColorProfileTransform::SrcProfile() const {
  return src_profile_;
}

const skcms_ICCProfile* ColorProfileTransform::DstProfile() const {
  return &dst_profile_;
}

void ImageDecoder::SetEmbeddedColorProfile(
    std::unique_ptr<ColorProfile> profile) {
  DCHECK(!IgnoresColorSpace());

  embedded_color_profile_ = std::move(profile);
  sk_image_color_space_ = nullptr;
  embedded_to_sk_image_transform_.reset();
}

ColorProfileTransform* ImageDecoder::ColorTransform() {
  UpdateSkImageColorSpaceAndTransform();
  return embedded_to_sk_image_transform_.get();
}

ColorProfileTransform::~ColorProfileTransform() = default;

sk_sp<SkColorSpace> ImageDecoder::ColorSpaceForSkImages() {
  UpdateSkImageColorSpaceAndTransform();
  return sk_image_color_space_;
}

void ImageDecoder::UpdateSkImageColorSpaceAndTransform() {
  if (color_behavior_ == ColorBehavior::kIgnore) {
    return;
  }

  // If `color_behavior_` is not ignore, then this function will always set
  // `sk_image_color_space_` to something non-nullptr, so, if it is non-nullptr,
  // then everything is up to date.
  if (sk_image_color_space_) {
    return;
  }

  if (color_behavior_ == ColorBehavior::kTag) {
    // Set `sk_image_color_space_` to the best SkColorSpace approximation
    // of `embedded_color_profile_`.
    if (embedded_color_profile_) {
      const skcms_ICCProfile* profile = embedded_color_profile_->GetProfile();

      // If the ICC profile has CICP data, prefer to use that.
      if (profile->has_CICP) {
        sk_image_color_space_ =
            skia::CICPGetSkColorSpace(profile->CICP.color_primaries,
                                      profile->CICP.transfer_characteristics,
                                      profile->CICP.matrix_coefficients,
                                      profile->CICP.video_full_range_flag,
                                      /*prefer_srgb_trfn=*/true);
        // A CICP profile's SkColorSpace is considered an exact representation
        // of `profile`, so don't create `embedded_to_sk_image_transform_`.
        if (sk_image_color_space_) {
          return;
        }
      }

      // If there was not CICP data, then use the ICC profile.
      DCHECK(!sk_image_color_space_);
      sk_image_color_space_ = SkColorSpace::Make(*profile);

      // If the embedded color space isn't supported by Skia, we will transform
      // to a supported color space using `embedded_to_sk_image_transform_` at
      // decode time.
      if (!sk_image_color_space_ && profile->has_toXYZD50) {
        // Preserve the gamut, but convert to a standard transfer function.
        skcms_ICCProfile with_srgb = *profile;
        skcms_SetTransferFunction(&with_srgb, skcms_sRGB_TransferFunction());
        sk_image_color_space_ = SkColorSpace::Make(with_srgb);
      }

      // For color spaces without an identifiable gamut, just default to sRGB.
      if (!sk_image_color_space_) {
        sk_image_color_space_ = SkColorSpace::MakeSRGB();
      }
    } else {
      // If there is no `embedded_color_profile_`, then assume that the content
      // was sRGB (and `embedded_to_sk_image_transform_` is not needed).
      sk_image_color_space_ = SkColorSpace::MakeSRGB();
      return;
    }
  } else {
    DCHECK(color_behavior_ == ColorBehavior::kTransformToSRGB);
    sk_image_color_space_ = SkColorSpace::MakeSRGB();

    // If there is no `embedded_color_profile_`, then assume the content was
    // sRGB  (and, as above, `embedded_to_sk_image_transform_` is not needed).
    if (!embedded_color_profile_) {
      return;
    }
  }

  // If we arrive here then we may need to create a transform from
  // `embedded_color_profile_` to `sk_image_color_space_`.
  DCHECK(embedded_color_profile_);
  DCHECK(sk_image_color_space_);

  const skcms_ICCProfile* src_profile = embedded_color_profile_->GetProfile();
  skcms_ICCProfile dst_profile;
  sk_image_color_space_->toProfile(&dst_profile);
  if (skcms_ApproximatelyEqualProfiles(src_profile, &dst_profile)) {
    return;
  }

  embedded_to_sk_image_transform_ =
      std::make_unique<ColorProfileTransform>(src_profile, &dst_profile);
}

bool ImageDecoder::CanReusePreviousFrameBuffer(wtf_size_t) const {
  return false;
}

}  // namespace blink
