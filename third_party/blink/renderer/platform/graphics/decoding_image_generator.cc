/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/decoding_image_generator.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

// static
std::unique_ptr<SkImageGenerator>
DecodingImageGenerator::CreateAsSkImageGenerator(sk_sp<SkData> data) {
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSkData(std::move(data));
  // We just need the size of the image, so we have to temporarily create an
  // ImageDecoder. Since we only need the size, the premul, high bit depth and
  // gamma settings don't really matter.
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      segment_reader, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::Ignore());
  if (!decoder || !decoder->IsSizeAvailable())
    return nullptr;

  const IntSize size = decoder->Size();
  const SkImageInfo info =
      SkImageInfo::MakeN32(size.Width(), size.Height(), kPremul_SkAlphaType,
                           decoder->ColorSpaceForSkImages());

  scoped_refptr<ImageFrameGenerator> frame = ImageFrameGenerator::Create(
      SkISize::Make(size.Width(), size.Height()), false,
      decoder->GetColorBehavior(), decoder->GetSupportedDecodeSizes());
  if (!frame)
    return nullptr;

  WebVector<FrameMetadata> frames;
  frames.emplace_back(FrameMetadata());
  cc::ImageHeaderMetadata image_metadata =
      decoder->MakeMetadataForDecodeAcceleration();
  image_metadata.all_data_received_prior_to_decode = true;
  sk_sp<DecodingImageGenerator> generator = DecodingImageGenerator::Create(
      std::move(frame), info, std::move(segment_reader), std::move(frames),
      PaintImage::GetNextContentId(), true /* all_data_received */,
      false /* can_yuv_decode */, image_metadata);
  return std::make_unique<SkiaPaintImageGenerator>(
      std::move(generator), PaintImage::kDefaultFrameIndex,
      PaintImage::kDefaultGeneratorClientId);
}

// static
sk_sp<DecodingImageGenerator> DecodingImageGenerator::Create(
    scoped_refptr<ImageFrameGenerator> frame_generator,
    const SkImageInfo& info,
    scoped_refptr<SegmentReader> data,
    WebVector<FrameMetadata> frames,
    PaintImage::ContentId content_id,
    bool all_data_received,
    bool can_yuv_decode,
    const cc::ImageHeaderMetadata& image_metadata) {
  return sk_sp<DecodingImageGenerator>(new DecodingImageGenerator(
      std::move(frame_generator), info, std::move(data), std::move(frames),
      content_id, all_data_received, can_yuv_decode, image_metadata));
}

DecodingImageGenerator::DecodingImageGenerator(
    scoped_refptr<ImageFrameGenerator> frame_generator,
    const SkImageInfo& info,
    scoped_refptr<SegmentReader> data,
    WebVector<FrameMetadata> frames,
    PaintImage::ContentId complete_frame_content_id,
    bool all_data_received,
    bool can_yuv_decode,
    const cc::ImageHeaderMetadata& image_metadata)
    : PaintImageGenerator(info, frames.ReleaseVector()),
      frame_generator_(std::move(frame_generator)),
      data_(std::move(data)),
      all_data_received_(all_data_received),
      can_yuv_decode_(can_yuv_decode),
      complete_frame_content_id_(complete_frame_content_id),
      image_metadata_(image_metadata) {}

DecodingImageGenerator::~DecodingImageGenerator() = default;

sk_sp<SkData> DecodingImageGenerator::GetEncodedData() const {
  TRACE_EVENT0("blink", "DecodingImageGenerator::refEncodedData");

  // getAsSkData() may require copying, but the clients of this function are
  // serializers, which want the data even if it requires copying, and even
  // if the data is incomplete. (Otherwise they would potentially need to
  // decode the partial image in order to re-encode it.)
  return data_->GetAsSkData();
}

bool DecodingImageGenerator::GetPixels(const SkImageInfo& dst_info,
                                       void* pixels,
                                       size_t row_bytes,
                                       size_t frame_index,
                                       PaintImage::GeneratorClientId client_id,
                                       uint32_t lazy_pixel_ref) {
  TRACE_EVENT2("blink", "DecodingImageGenerator::getPixels", "frame index",
               static_cast<int>(frame_index), "client_id", client_id);

  // Implementation only supports decoding to a supported size.
  if (dst_info.dimensions() != GetSupportedDecodeSize(dst_info.dimensions())) {
    return false;
  }

  // Color type can be N32 or F16. Otherwise, decode to N32 and convert to
  // the requested color type from N32.
  SkImageInfo target_info = dst_info;
  char* memory = static_cast<char*>(pixels);
  std::unique_ptr<char[]> memory_ref_ptr;
  size_t adjusted_row_bytes = row_bytes;
  if ((target_info.colorType() != kN32_SkColorType) &&
      (target_info.colorType() != kRGBA_F16_SkColorType)) {
    target_info = target_info.makeColorType(kN32_SkColorType);
    // row_bytes is the size of scanline, so it should be >= info.minRowBytes().
    DCHECK(row_bytes >= dst_info.minRowBytes());
    // row_bytes must be a multiple of dst_info.bytesPerPixel().
    DCHECK_EQ(0ul, row_bytes % dst_info.bytesPerPixel());
    adjusted_row_bytes =
        target_info.bytesPerPixel() * (row_bytes / dst_info.bytesPerPixel());
    memory_ref_ptr.reset(new char[target_info.computeMinByteSize()]);
    memory = memory_ref_ptr.get();
  }

  // Skip the check for alphaType.  blink::ImageFrame may have changed the
  // owning SkBitmap to kOpaque_SkAlphaType after fully decoding the image
  // frame, so if we see a request for opaque, that is ok even if our initial
  // alpha type was not opaque.

  // Pass decodeColorSpace to the decoder.  That is what we can expect the
  // output to be.
  sk_sp<SkColorSpace> decode_color_space = GetSkImageInfo().refColorSpace();
  SkImageInfo decode_info = target_info.makeColorSpace(decode_color_space);

  const bool needs_color_xform = !ApproximatelyEqualSkColorSpaces(
      decode_color_space, target_info.refColorSpace());
  ImageDecoder::AlphaOption alpha_option = ImageDecoder::kAlphaPremultiplied;
  if (needs_color_xform && !decode_info.isOpaque()) {
    alpha_option = ImageDecoder::kAlphaNotPremultiplied;
    decode_info = decode_info.makeAlphaType(kUnpremul_SkAlphaType);
  }

  bool decoded = false;
  {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                 "Decode LazyPixelRef", "LazyPixelRef", lazy_pixel_ref);
    decoded = frame_generator_->DecodeAndScale(
        data_.get(), all_data_received_, frame_index, decode_info, memory,
        adjusted_row_bytes, alpha_option, client_id);
  }

  if (decoded && needs_color_xform) {
    TRACE_EVENT0("blink", "DecodingImageGenerator::getPixels - apply xform");
    SkPixmap src(decode_info, memory, adjusted_row_bytes);
    decoded = src.readPixels(target_info, memory, adjusted_row_bytes);
    DCHECK(decoded);
  }

  // Convert the color type to the requested one if necessary
  if (decoded && target_info.colorType() != dst_info.colorType()) {
    // Convert the color type by readPixels if dithering is not necessary
    // (readPixels is potentially cheaper than a full-blown drawBitmap).
    if (SkColorTypeBytesPerPixel(target_info.colorType()) <=
        SkColorTypeBytesPerPixel(dst_info.colorType())) {
      decoded = SkPixmap{target_info, memory, adjusted_row_bytes}.readPixels(
          SkPixmap{dst_info, pixels, row_bytes});
      DCHECK(decoded);
    } else {  // Do dithering by drawBitmap() if dithering is necessary
      auto canvas = SkCanvas::MakeRasterDirect(dst_info, pixels, row_bytes);
      DCHECK(canvas);

      SkPaint paint;
      paint.setDither(true);
      paint.setBlendMode(SkBlendMode::kSrc);

      SkBitmap bitmap;
      decoded = bitmap.installPixels(target_info, memory, adjusted_row_bytes);
      DCHECK(decoded);

      canvas->drawBitmap(bitmap, 0, 0, &paint);
    }
  }
  return decoded;
}

bool DecodingImageGenerator::QueryYUVA8(
    SkYUVASizeInfo* size_info,
    SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
    SkYUVColorSpace* color_space) const {
  if (!can_yuv_decode_)
    return false;

  TRACE_EVENT0("blink", "DecodingImageGenerator::queryYUVA8");

  // Indicate that we have three separate planes
  indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
  indices[SkYUVAIndex::kU_Index] = {1, SkColorChannel::kR};
  indices[SkYUVAIndex::kV_Index] = {2, SkColorChannel::kR};
  indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};

  DCHECK(all_data_received_);
  return frame_generator_->GetYUVComponentSizes(data_.get(), size_info,
                                                color_space);
}

bool DecodingImageGenerator::GetYUVA8Planes(const SkYUVASizeInfo& size_info,
                                            const SkYUVAIndex indices[4],
                                            void* planes[3],
                                            size_t frame_index,
                                            uint32_t lazy_pixel_ref) {
  // TODO(crbug.com/943519): YUV decoding does not currently support incremental
  // decoding. See comment in image_frame_generator.h.
  DCHECK(can_yuv_decode_);
  DCHECK(all_data_received_);

  TRACE_EVENT0("blink", "DecodingImageGenerator::getYUVA8Planes");
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "Decode LazyPixelRef", "LazyPixelRef", lazy_pixel_ref);

  // Verify sizes and indices
  for (int i = 0; i < 3; ++i) {
    if (size_info.fSizes[i].isEmpty() || !size_info.fWidthBytes[i]) {
      return false;
    }
  }
  if (!size_info.fSizes[3].isEmpty() || size_info.fWidthBytes[3]) {
    return false;
  }
  int numPlanes;
  if (!SkYUVAIndex::AreValidIndices(indices, &numPlanes) || numPlanes != 3) {
    return false;
  }

  bool decoded =
      frame_generator_->DecodeToYUV(data_.get(), frame_index, size_info.fSizes,
                                    planes, size_info.fWidthBytes);
  return decoded;
}

SkISize DecodingImageGenerator::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  return frame_generator_->GetSupportedDecodeSize(requested_size);
}

PaintImage::ContentId DecodingImageGenerator::GetContentIdForFrame(
    size_t frame_index) const {
  DCHECK_LT(frame_index, GetFrameMetadata().size());

  // If we have all the data for the image, or this particular frame, we can
  // consider the decoded frame constant.
  if (all_data_received_ || GetFrameMetadata().at(frame_index).complete)
    return complete_frame_content_id_;

  return PaintImageGenerator::GetContentIdForFrame(frame_index);
}

const cc::ImageHeaderMetadata*
DecodingImageGenerator::GetMetadataForDecodeAcceleration() const {
  return &image_metadata_;
}

}  // namespace blink
