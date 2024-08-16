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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/decoding_image_generator.h"

#include <memory>
#include <utility>

#include "base/containers/heap_array.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace {
class ScopedSegmentReaderDataLocker {
  STACK_ALLOCATED();

 public:
  explicit ScopedSegmentReaderDataLocker(blink::SegmentReader* segment_reader)
      : segment_reader_(segment_reader) {
    segment_reader_->LockData();
  }
  ~ScopedSegmentReaderDataLocker() { segment_reader_->UnlockData(); }

 private:
  blink::SegmentReader* const segment_reader_;
};
}  // namespace

namespace blink {

// static
std::unique_ptr<SkImageGenerator>
DecodingImageGenerator::CreateAsSkImageGenerator(sk_sp<SkData> data) {
  // This image generator is used only by code in Skia, which in practice means
  // out of process printing deserialization (MSKP) and a few odds and ends.
  // Blink side code uses DecodingImageGenerator::Create directly instead.
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSkData(std::move(data));
  const bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      segment_reader, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::kTag,
      cc::AuxImage::kDefault, Platform::GetMaxDecodedImageBytes());
  if (!decoder || !decoder->IsSizeAvailable())
    return nullptr;

  const gfx::Size size = decoder->Size();
  const SkImageInfo info =
      SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType,
                           decoder->ColorSpaceForSkImages());

  scoped_refptr<ImageFrameGenerator> frame = ImageFrameGenerator::Create(
      SkISize::Make(size.width(), size.height()), false,
      decoder->GetColorBehavior(), cc::AuxImage::kDefault,
      decoder->GetSupportedDecodeSizes());
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

bool DecodingImageGenerator::GetPixels(SkPixmap dst_pixmap,
                                       size_t frame_index,
                                       PaintImage::GeneratorClientId client_id,
                                       uint32_t lazy_pixel_ref) {
  TRACE_EVENT2("blink", "DecodingImageGenerator::getPixels", "frame index",
               static_cast<int>(frame_index), "client_id", client_id);
  const SkImageInfo& dst_info = dst_pixmap.info();

  // Implementation only supports decoding to a supported size.
  if (dst_info.dimensions() != GetSupportedDecodeSize(dst_info.dimensions())) {
    return false;
  }

  // Color type can be N32 or F16. Otherwise, decode to N32 and convert to
  // the requested color type from N32.
  SkImageInfo target_info = dst_info;
  char* memory = static_cast<char*>(dst_pixmap.writable_addr());
  base::HeapArray<char> adjusted_memory;
  size_t adjusted_row_bytes = dst_pixmap.rowBytes();
  if ((target_info.colorType() != kN32_SkColorType) &&
      (target_info.colorType() != kRGBA_F16_SkColorType)) {
    target_info = target_info.makeColorType(kN32_SkColorType);
    // dst_info.rowBytes is the size of scanline, so it should be >=
    // info.minRowBytes().
    DCHECK(dst_pixmap.rowBytes() >= dst_info.minRowBytes());
    // dst_info.rowBytes must be a multiple of dst_info.bytesPerPixel().
    DCHECK_EQ(0ul, dst_pixmap.rowBytes() % dst_info.bytesPerPixel());
    adjusted_row_bytes = target_info.bytesPerPixel() *
                         (dst_pixmap.rowBytes() / dst_info.bytesPerPixel());
    adjusted_memory =
        base::HeapArray<char>::Uninit(target_info.computeMinByteSize());
    memory = adjusted_memory.data();
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
  if (needs_color_xform && !decode_info.isOpaque()) {
    decode_info = decode_info.makeAlphaType(kUnpremul_SkAlphaType);
  } else {
    DCHECK(decode_info.alphaType() != kUnpremul_SkAlphaType);
  }
  SkPixmap decode_pixmap(decode_info, memory, adjusted_row_bytes);

  bool decoded = false;
  {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                 "Decode LazyPixelRef", "LazyPixelRef", lazy_pixel_ref);

    ScopedSegmentReaderDataLocker lock_data(data_.get());
    decoded = frame_generator_->DecodeAndScale(
        data_.get(), all_data_received_, static_cast<wtf_size_t>(frame_index),
        decode_pixmap, client_id);
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
          dst_pixmap);
      DCHECK(decoded);
    } else {  // Do dithering by drawBitmap() if dithering is necessary
      auto canvas = SkCanvas::MakeRasterDirect(
          dst_pixmap.info(), dst_pixmap.writable_addr(), dst_pixmap.rowBytes());
      DCHECK(canvas);

      SkPaint paint;
      paint.setDither(true);
      paint.setBlendMode(SkBlendMode::kSrc);

      SkBitmap bitmap;
      decoded = bitmap.installPixels(target_info, memory, adjusted_row_bytes);
      DCHECK(decoded);

      canvas->drawImage(bitmap.asImage(), 0, 0, SkSamplingOptions(), &paint);
    }
  }
  return decoded;
}

bool DecodingImageGenerator::QueryYUVA(
    const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
    SkYUVAPixmapInfo* yuva_pixmap_info) const {
  if (!can_yuv_decode_)
    return false;

  TRACE_EVENT0("blink", "DecodingImageGenerator::QueryYUVAInfo");

  DCHECK(all_data_received_);

  ScopedSegmentReaderDataLocker lock_data(data_.get());
  return frame_generator_->GetYUVAInfo(data_.get(), supported_data_types,
                                       yuva_pixmap_info);
}

bool DecodingImageGenerator::GetYUVAPlanes(
    const SkYUVAPixmaps& pixmaps,
    size_t frame_index,
    uint32_t lazy_pixel_ref,
    PaintImage::GeneratorClientId client_id) {
  // TODO(crbug.com/943519): YUV decoding does not currently support incremental
  // decoding. See comment in image_frame_generator.h.
  DCHECK(can_yuv_decode_);
  DCHECK(all_data_received_);

  TRACE_EVENT0("blink", "DecodingImageGenerator::GetYUVAPlanes");
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "Decode LazyPixelRef", "LazyPixelRef", lazy_pixel_ref);

  SkISize plane_sizes[3];
  wtf_size_t plane_row_bytes[3];
  void* plane_addrs[3];

  // Verify sizes and extract DecodeToYUV parameters
  for (int i = 0; i < 3; ++i) {
    const SkPixmap& plane = pixmaps.plane(i);
    if (plane.dimensions().isEmpty() || !plane.rowBytes())
      return false;
    if (plane.colorType() != pixmaps.plane(0).colorType())
      return false;
    plane_sizes[i] = plane.dimensions();
    plane_row_bytes[i] = base::checked_cast<wtf_size_t>(plane.rowBytes());
    plane_addrs[i] = plane.writable_addr();
  }
  if (!pixmaps.plane(3).dimensions().isEmpty()) {
    return false;
  }

  ScopedSegmentReaderDataLocker lock_data(data_.get());
  return frame_generator_->DecodeToYUV(
      data_.get(), static_cast<wtf_size_t>(frame_index),
      pixmaps.plane(0).colorType(), plane_sizes, plane_addrs, plane_row_bytes,
      client_id);
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
