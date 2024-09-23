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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"

#include <memory>
#include <utility>

#include "base/not_fatal_until.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/image_decoder_wrapper.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

SkYUVAInfo::Subsampling SubsamplingToSkiaSubsampling(
    cc::YUVSubsampling subsampling) {
  switch (subsampling) {
    case cc::YUVSubsampling::k410:
      return SkYUVAInfo::Subsampling::k410;
    case cc::YUVSubsampling::k411:
      return SkYUVAInfo::Subsampling::k411;
    case cc::YUVSubsampling::k420:
      return SkYUVAInfo::Subsampling::k420;
    case cc::YUVSubsampling::k422:
      return SkYUVAInfo::Subsampling::k422;
    case cc::YUVSubsampling::k440:
      return SkYUVAInfo::Subsampling::k440;
    case cc::YUVSubsampling::k444:
      return SkYUVAInfo::Subsampling::k444;
    case cc::YUVSubsampling::kUnknown:
      return SkYUVAInfo::Subsampling::kUnknown;
  }
}

static bool UpdateYUVAInfoSubsamplingAndWidthBytes(
    ImageDecoder* decoder,
    SkYUVAInfo::Subsampling* subsampling,
    size_t component_width_bytes[SkYUVAInfo::kMaxPlanes]) {
  SkYUVAInfo::Subsampling tempSubsampling =
      SubsamplingToSkiaSubsampling(decoder->GetYUVSubsampling());
  if (tempSubsampling == SkYUVAInfo::Subsampling::kUnknown) {
    return false;
  }
  *subsampling = tempSubsampling;
  component_width_bytes[0] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kY);
  component_width_bytes[1] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kU);
  component_width_bytes[2] = decoder->DecodedYUVWidthBytes(cc::YUVIndex::kV);
  // TODO(crbug/910276): Alpha plane is currently unsupported.
  component_width_bytes[3] = 0;
  return true;
}

ImageFrameGenerator::ImageFrameGenerator(const SkISize& full_size,
                                         bool is_multi_frame,
                                         ColorBehavior color_behavior,
                                         cc::AuxImage aux_image,
                                         Vector<SkISize> supported_sizes)
    : full_size_(full_size),
      decoder_color_behavior_(color_behavior),
      aux_image_(aux_image),
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
    wtf_size_t index,
    const SkPixmap& pixmap,
    cc::PaintImage::GeneratorClientId client_id) {
  {
    base::AutoLock lock(generator_lock_);
    if (decode_failed_)
      return false;
    RecordWhetherMultiDecoded(client_id);
  }

  TRACE_EVENT1("blink", "ImageFrameGenerator::decodeAndScale", "generator",
               static_cast<void*>(this));

  // This implementation does not support arbitrary scaling so check the
  // requested size.
  const SkISize scaled_size = pixmap.dimensions();
  CHECK(GetSupportedDecodeSize(scaled_size) == scaled_size);

  wtf_size_t frame_count = 0u;
  bool has_alpha = true;

  // |decode_failed| indicates a failure due to a corrupt image.
  bool decode_failed = false;
  // |current_decode_succeeded| indicates a failure to decode the current frame.
  // Its possible to have a valid but fail to decode a frame in the case where
  // we don't have enough data to decode this particular frame yet.
  bool current_decode_succeeded = false;
  {
    // Lock the mutex, so only one thread can use the decoder at once.
    ClientAutoLock lock(this, client_id);
    ImageDecoderWrapper decoder_wrapper(this, data, pixmap,
                                        decoder_color_behavior_, aux_image_,
                                        index, all_data_received, client_id);
    current_decode_succeeded = decoder_wrapper.Decode(
        image_decoder_factory_.get(), &frame_count, &has_alpha);
    decode_failed = decoder_wrapper.decode_failed();
  }

  base::AutoLock lock(generator_lock_);
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

bool ImageFrameGenerator::DecodeToYUV(
    SegmentReader* data,
    wtf_size_t index,
    SkColorType color_type,
    const SkISize component_sizes[cc::kNumYUVPlanes],
    void* planes[cc::kNumYUVPlanes],
    const wtf_size_t row_bytes[cc::kNumYUVPlanes],
    cc::PaintImage::GeneratorClientId client_id) {
  base::AutoLock lock(generator_lock_);
  DCHECK_EQ(index, 0u);

  RecordWhetherMultiDecoded(client_id);

  // TODO (scroggo): The only interesting thing this uses from the
  // ImageFrameGenerator is |decode_failed_|. Move this into
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
      ImageDecoder::kDefaultBitDepth, decoder_color_behavior_, aux_image_,
      Platform::GetMaxDecodedImageBytes());
  // getYUVComponentSizes was already called and was successful, so
  // ImageDecoder::create must succeed.
  DCHECK(decoder);

  std::unique_ptr<ImagePlanes> image_planes =
      std::make_unique<ImagePlanes>(planes, row_bytes, color_type);
  // TODO(crbug.com/943519): Don't forget to initialize planes to black or
  // transparent for incremental decoding.
  decoder->SetImagePlanes(std::move(image_planes));

  DCHECK(decoder->CanDecodeToYUV());

  {
    // This is the YUV analog of ImageFrameGenerator::decode.
    TRACE_EVENT0("blink,benchmark", "ImageFrameGenerator::decodeToYUV");
    decoder->DecodeToYUV();
  }

  // Display a complete scan if available, even if decoding fails.
  if (decoder->HasDisplayableYUVData()) {
    // TODO(crbug.com/910276): Set this properly for alpha support.
    SetHasAlpha(index, false);
    return true;
  }

  // Currently if there is no displayable data, the decoder always fails.
  // This may not be the case once YUV supports incremental decoding
  // (crbug.com/943519).
  if (decoder->Failed()) {
    yuv_decoding_failed_ = true;
  }

  return false;
}

void ImageFrameGenerator::SetHasAlpha(wtf_size_t index, bool has_alpha) {
  generator_lock_.AssertAcquired();

  if (index >= has_alpha_.size()) {
    const wtf_size_t old_size = has_alpha_.size();
    has_alpha_.resize(index + 1);
    for (wtf_size_t i = old_size; i < has_alpha_.size(); ++i)
      has_alpha_[i] = true;
  }
  has_alpha_[index] = has_alpha;
}

void ImageFrameGenerator::RecordWhetherMultiDecoded(
    cc::PaintImage::GeneratorClientId client_id) {
  generator_lock_.AssertAcquired();

  if (client_id == cc::PaintImage::kDefaultGeneratorClientId)
    return;

  if (last_client_id_ == cc::PaintImage::kDefaultGeneratorClientId) {
    DCHECK(!has_logged_multi_clients_);
    last_client_id_ = client_id;
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.ImageDecoders.ImageHasMultipleGeneratorClientIds",
        DecodeTimesType::kRequestByAtLeastOneClient);
  } else if (last_client_id_ != client_id && !has_logged_multi_clients_) {
    has_logged_multi_clients_ = true;
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.ImageDecoders.ImageHasMultipleGeneratorClientIds",
        DecodeTimesType::kRequestByMoreThanOneClient);
  }
}

bool ImageFrameGenerator::HasAlpha(wtf_size_t index) {
  base::AutoLock lock(generator_lock_);

  if (index < has_alpha_.size())
    return has_alpha_[index];
  return true;
}

bool ImageFrameGenerator::GetYUVAInfo(
    SegmentReader* data,
    const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
    SkYUVAPixmapInfo* info) {
  TRACE_EVENT2("blink", "ImageFrameGenerator::GetYUVAInfo", "width",
               full_size_.width(), "height", full_size_.height());

  base::AutoLock lock(generator_lock_);

  if (yuv_decoding_failed_)
    return false;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, /*data_complete=*/true, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, decoder_color_behavior_, aux_image_,
      Platform::GetMaxDecodedImageBytes());
  DCHECK(decoder);

  DCHECK(decoder->CanDecodeToYUV())
      << decoder->FilenameExtension() << " image decoder";
  SkYUVAInfo::Subsampling subsampling;
  size_t width_bytes[SkYUVAInfo::kMaxPlanes];
  if (!UpdateYUVAInfoSubsamplingAndWidthBytes(decoder.get(), &subsampling,
                                              width_bytes)) {
    return false;
  }
  SkYUVAInfo yuva_info(full_size_, SkYUVAInfo::PlaneConfig::kY_U_V, subsampling,
                       decoder->GetYUVColorSpace());
  SkYUVAPixmapInfo::DataType dataType;
  if (decoder->GetYUVBitDepth() > 8) {
    if (supported_data_types.supported(SkYUVAInfo::PlaneConfig::kY_U_V,
                                       SkYUVAPixmapInfo::DataType::kUnorm16)) {
      dataType = SkYUVAPixmapInfo::DataType::kUnorm16;
    } else if (supported_data_types.supported(
                   SkYUVAInfo::PlaneConfig::kY_U_V,
                   SkYUVAPixmapInfo::DataType::kFloat16)) {
      dataType = SkYUVAPixmapInfo::DataType::kFloat16;
    } else {
      return false;
    }
  } else if (supported_data_types.supported(
                 SkYUVAInfo::PlaneConfig::kY_U_V,
                 SkYUVAPixmapInfo::DataType::kUnorm8)) {
    dataType = SkYUVAPixmapInfo::DataType::kUnorm8;
  } else {
    return false;
  }
  *info = SkYUVAPixmapInfo(yuva_info, dataType, width_bytes);
  DCHECK(info->isSupported(supported_data_types));

  return true;
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

ImageFrameGenerator::ClientAutoLock::ClientAutoLock(
    ImageFrameGenerator* generator,
    cc::PaintImage::GeneratorClientId client_id)
    : generator_(generator), client_id_(client_id) {
  {
    base::AutoLock lock(generator_->generator_lock_);
    auto it = generator_->lock_map_.find(client_id_);
    ClientLock* client_lock;
    if (it == generator_->lock_map_.end()) {
      auto result = generator_->lock_map_.insert(
          client_id_, std::make_unique<ClientLock>());
      client_lock = result.stored_value->value.get();
    } else {
      client_lock = it->value.get();
    }
    client_lock->ref_count++;
    lock_ = &client_lock->lock;
  }

  lock_->Acquire();
}

ImageFrameGenerator::ClientAutoLock::~ClientAutoLock() {
  lock_->Release();

  base::AutoLock lock(generator_->generator_lock_);
  auto it = generator_->lock_map_.find(client_id_);
  CHECK(it != generator_->lock_map_.end(), base::NotFatalUntil::M130);
  it->value->ref_count--;

  if (it->value->ref_count == 0)
    generator_->lock_map_.erase(it);
}

}  // namespace blink
