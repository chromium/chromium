/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/image_decoder_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkYUVASizeInfo.h"

namespace blink {

static bool UpdateYUVComponentSizes(ImageDecoder* decoder,
                                    SkISize component_sizes[4],
                                    size_t component_width_bytes[4]) {
  DCHECK(decoder->CanDecodeToYUV());
  // Initialize sizes for decoder if not already set.
  bool size_available = decoder->IsSizeAvailable();
  DCHECK(size_available);

  for (int yuv_index = 0; yuv_index < 3; ++yuv_index) {
    IntSize size = decoder->DecodedYUVSize(yuv_index);
    component_sizes[yuv_index].set(size.Width(), size.Height());
    component_width_bytes[yuv_index] = decoder->DecodedYUVWidthBytes(yuv_index);
  }
  // TODO(crbug/910276): Alpha plane is currently unsupported.
  component_sizes[3] = SkISize::MakeEmpty();
  component_width_bytes[3] = 0;

  return true;
}

ImageFrameGenerator::ImageFrameGenerator(const SkISize& full_size,
                                         bool is_multi_frame,
                                         const ColorBehavior& color_behavior,
                                         Vector<SkISize> supported_sizes)
    : full_size_(full_size),
      decoder_color_behavior_(color_behavior),
      is_multi_frame_(is_multi_frame),
      supported_sizes_(std::move(supported_sizes)) {
#if DCHECK_IS_ON()
  // Verify that sizes are in an increasing order, since
  // GetSupportedDecodeSize() depends on it.
  SkISize last_size = SkISize::MakeEmpty();
  for (auto& size : supported_sizes_) {
    DCHECK_GE(size.width(), last_size.width());
    DCHECK_GE(size.height(), last_size.height());
  }
#endif
}

ImageFrameGenerator::~ImageFrameGenerator() {
  // We expect all image decoders to be unlocked and catch with DCHECKs if not.
  ImageDecodingStore::Instance().RemoveCacheIndexedByGenerator(this);
}

bool ImageFrameGenerator::DecodeAndScale(
    SegmentReader* data,
    bool all_data_received,
    size_t index,
    const SkImageInfo& info,
    void* pixels,
    size_t row_bytes,
    ImageDecoder::AlphaOption alpha_option,
    cc::PaintImage::GeneratorClientId client_id) {
  {
    MutexLocker lock(generator_mutex_);
    if (decode_failed_)
      return false;
  }

  TRACE_EVENT1("blink", "ImageFrameGenerator::decodeAndScale", "generator",
               this);

  // This implementation does not support arbitrary scaling so check the
  // requested size.
  SkISize scaled_size = SkISize::Make(info.width(), info.height());
  CHECK(GetSupportedDecodeSize(scaled_size) == scaled_size);

  ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option =
      ImageDecoder::kDefaultBitDepth;
  if (info.colorType() == kRGBA_F16_SkColorType) {
    high_bit_depth_decoding_option = ImageDecoder::kHighBitDepthToHalfFloat;
  }

  size_t frame_count = 0u;
  bool has_alpha = true;

  // |decode_failed| indicates a failure due to a corrupt image.
  bool decode_failed = false;
  // |current_decode_succeeded| indicates a failure to decode the current frame.
  // Its possible to have a valid but fail to decode a frame in the case where
  // we don't have enough data to decode this particular frame yet.
  bool current_decode_succeeded = false;
  {
    // Lock the mutex, so only one thread can use the decoder at once.
    ClientMutexLocker lock(this, client_id);
    ImageDecoderWrapper decoder_wrapper(
        this, data, scaled_size, alpha_option, decoder_color_behavior_,
        high_bit_depth_decoding_option, index, info, pixels, row_bytes,
        all_data_received, client_id);
    current_decode_succeeded = decoder_wrapper.Decode(
        image_decoder_factory_.get(), &frame_count, &has_alpha);
    decode_failed = decoder_wrapper.decode_failed();
  }

  MutexLocker lock(generator_mutex_);
  decode_failed_ = decode_failed;
  if (decode_failed_) {
    DCHECK(!current_decode_succeeded);
    return false;
  }

  if (!current_decode_succeeded)
    return false;

  SetHasAlpha(index, has_alpha);
  if (frame_count != 0u)
    frame_count_ = frame_count;

  return true;
}

bool ImageFrameGenerator::DecodeToYUV(SegmentReader* data,
                                      size_t index,
                                      const SkISize component_sizes[3],
                                      void* planes[3],
                                      const size_t row_bytes[3]) {
  MutexLocker lock(generator_mutex_);
  DCHECK_EQ(index, 0u);

  // TODO (scroggo): The only interesting thing this uses from the
  // ImageFrameGenerator is m_decodeFailed. Move this into
  // DecodingImageGenerator, which is the only class that calls it.
  if (decode_failed_ || yuv_decoding_failed_)
    return false;

  if (!planes || !planes[0] || !planes[1] || !planes[2] || !row_bytes ||
      !row_bytes[0] || !row_bytes[1] || !row_bytes[2]) {
    return false;
  }
  const bool all_data_received = true;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, all_data_received, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, decoder_color_behavior_);
  // getYUVComponentSizes was already called and was successful, so
  // ImageDecoder::create must succeed.
  DCHECK(decoder);

  std::unique_ptr<ImagePlanes> image_planes =
      std::make_unique<ImagePlanes>(planes, row_bytes);
  // TODO(crbug.com/943519): Don't forget to initialize planes to black or
  // transparent for incremental decoding.
  decoder->SetImagePlanes(std::move(image_planes));

  DCHECK(decoder->CanDecodeToYUV());

  {
    // This is the YUV analog of ImageFrameGenerator::decode.
    TRACE_EVENT0("blink,benchmark", "ImageFrameGenerator::decodeToYUV");
    decoder->DecodeToYUV();
  }

  if (!decoder->Failed()) {
    // TODO(crbug.com/910276): Set this properly for alpha support.
    SetHasAlpha(index, false);
    return true;
  }

  DCHECK(decoder->Failed());
  yuv_decoding_failed_ = true;
  return false;
}

void ImageFrameGenerator::SetHasAlpha(size_t index, bool has_alpha) {
  generator_mutex_.AssertAcquired();

  if (index >= has_alpha_.size()) {
    const size_t old_size = has_alpha_.size();
    has_alpha_.resize(index + 1);
    for (size_t i = old_size; i < has_alpha_.size(); ++i)
      has_alpha_[i] = true;
  }
  has_alpha_[index] = has_alpha;
}

bool ImageFrameGenerator::HasAlpha(size_t index) {
  MutexLocker lock(generator_mutex_);

  if (index < has_alpha_.size())
    return has_alpha_[index];
  return true;
}

bool ImageFrameGenerator::GetYUVComponentSizes(
    SegmentReader* data,
    SkYUVASizeInfo* size_info,
    SkYUVColorSpace* yuv_color_space) {
  TRACE_EVENT2("blink", "ImageFrameGenerator::getYUVComponentSizes", "width",
               full_size_.width(), "height", full_size_.height());

  MutexLocker lock(generator_mutex_);

  if (yuv_decoding_failed_)
    return false;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, true /* data_complete */, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, decoder_color_behavior_);
  DCHECK(decoder);

  DCHECK(decoder->CanDecodeToYUV());
  *yuv_color_space = decoder->GetYUVColorSpace();

  return UpdateYUVComponentSizes(decoder.get(), size_info->fSizes,
                                 size_info->fWidthBytes);
}

SkISize ImageFrameGenerator::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  for (auto& size : supported_sizes_) {
    if (size.width() >= requested_size.width() &&
        size.height() >= requested_size.height()) {
      return size;
    }
  }
  return full_size_;
}

ImageFrameGenerator::ClientMutexLocker::ClientMutexLocker(
    ImageFrameGenerator* generator,
    cc::PaintImage::GeneratorClientId client_id)
    : generator_(generator), client_id_(client_id) {
  {
    MutexLocker lock(generator_->generator_mutex_);
    auto it = generator_->mutex_map_.find(client_id_);
    ClientMutex* client_mutex;
    if (it == generator_->mutex_map_.end()) {
      auto result = generator_->mutex_map_.insert(
          client_id_, std::make_unique<ClientMutex>());
      client_mutex = result.stored_value->value.get();
    } else {
      client_mutex = it->value.get();
    }
    client_mutex->ref_count++;
    mutex_ = &client_mutex->mutex;
  }

  mutex_->lock();
}

ImageFrameGenerator::ClientMutexLocker::~ClientMutexLocker() {
  mutex_->unlock();

  MutexLocker lock(generator_->generator_mutex_);
  auto it = generator_->mutex_map_.find(client_id_);
  DCHECK(it != generator_->mutex_map_.end());
  it->value->ref_count--;

  if (it->value->ref_count == 0)
    generator_->mutex_map_.erase(it);
}

}  // namespace blink
