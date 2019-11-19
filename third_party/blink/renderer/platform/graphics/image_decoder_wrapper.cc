// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/image_decoder_wrapper.h"

#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"

namespace blink {
namespace {

bool CompatibleInfo(const SkImageInfo& src, const SkImageInfo& dst) {
  if (src == dst)
    return true;

  // It is legal to write kOpaque_SkAlphaType pixels into a kPremul_SkAlphaType
  // buffer. This can happen when DeferredImageDecoder allocates an
  // kOpaque_SkAlphaType image generator based on cached frame info, while the
  // ImageFrame-allocated dest bitmap stays kPremul_SkAlphaType.
  if (src.alphaType() == kOpaque_SkAlphaType &&
      dst.alphaType() == kPremul_SkAlphaType) {
    const SkImageInfo& tmp = src.makeAlphaType(kPremul_SkAlphaType);
    return tmp == dst;
  }

  return false;
}

// Creates a SkPixelRef such that the memory for pixels is given by an external
// body. This is used to write directly to the memory given by Skia during
// decoding.
class ExternalMemoryAllocator final : public SkBitmap::Allocator {
  USING_FAST_MALLOC(ExternalMemoryAllocator);

 public:
  ExternalMemoryAllocator(const SkImageInfo& info,
                          void* pixels,
                          size_t row_bytes)
      : info_(info), pixels_(pixels), row_bytes_(row_bytes) {}

  bool allocPixelRef(SkBitmap* dst) override {
    const SkImageInfo& info = dst->info();
    if (kUnknown_SkColorType == info.colorType())
      return false;

    if (!CompatibleInfo(info_, info) || row_bytes_ != dst->rowBytes())
      return false;

    return dst->installPixels(info, pixels_, row_bytes_);
  }

 private:
  SkImageInfo info_;
  void* pixels_;
  size_t row_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ExternalMemoryAllocator);
};

}  // namespace

ImageDecoderWrapper::ImageDecoderWrapper(
    ImageFrameGenerator* generator,
    SegmentReader* data,
    const SkISize& scaled_size,
    ImageDecoder::AlphaOption alpha_option,
    ColorBehavior decoder_color_behavior,
    ImageDecoder::HighBitDepthDecodingOption decoding_option,
    size_t index,
    const SkImageInfo& info,
    void* pixels,
    size_t row_bytes,
    bool all_data_received,
    cc::PaintImage::GeneratorClientId client_id)
    : generator_(generator),
      data_(data),
      scaled_size_(scaled_size),
      alpha_option_(alpha_option),
      decoder_color_behavior_(decoder_color_behavior),
      decoding_option_(decoding_option),
      frame_index_(index),
      info_(info),
      pixels_(pixels),
      row_bytes_(row_bytes),
      all_data_received_(all_data_received),
      client_id_(client_id) {}

ImageDecoderWrapper::~ImageDecoderWrapper() = default;

bool ImageDecoderWrapper::Decode(ImageDecoderFactory* factory,
                                 size_t* frame_count,
                                 bool* has_alpha) {
  DCHECK(frame_count);
  DCHECK(has_alpha);

  ImageDecoder* decoder = nullptr;
  std::unique_ptr<ImageDecoder> new_decoder;

  const bool resume_decoding = ImageDecodingStore::Instance().LockDecoder(
      generator_, scaled_size_, alpha_option_, client_id_, &decoder);
  DCHECK(!resume_decoding || decoder);

  if (resume_decoding) {
    decoder->SetData(data_, all_data_received_);
  } else {
    new_decoder = CreateDecoderWithData(factory);
    if (!new_decoder)
      return false;
    decoder = new_decoder.get();
  }

  // For multi-frame image decoders, we need to know how many frames are
  // in that image in order to release the decoder when all frames are
  // decoded. frameCount() is reliable only if all data is received and set in
  // decoder, particularly with GIF.
  if (all_data_received_)
    *frame_count = decoder->FrameCount();

  const bool decode_to_external_memory =
      ShouldDecodeToExternalMemory(*frame_count, resume_decoding);

  ExternalMemoryAllocator external_memory_allocator(info_, pixels_, row_bytes_);
  if (decode_to_external_memory)
    decoder->SetMemoryAllocator(&external_memory_allocator);
  ImageFrame* frame = nullptr;
  {
    // This trace event is important since it is used by telemetry scripts to
    // measure the decode time.
    TRACE_EVENT0("blink,benchmark", "ImageFrameGenerator::decode");
    frame = decoder->DecodeFrameBufferAtIndex(frame_index_);
  }
  // SetMemoryAllocator() can try to access decoder's data, so we have to
  // clear it before clearing SegmentReader.
  if (decode_to_external_memory)
    decoder->SetMemoryAllocator(nullptr);
  // Verify we have the only ref-count.
  DCHECK(external_memory_allocator.unique());

  decoder->SetData(scoped_refptr<SegmentReader>(nullptr), false);
  decoder->ClearCacheExceptFrame(frame_index_);

  const bool has_decoded_frame =
      frame && frame->GetStatus() != ImageFrame::kFrameEmpty &&
      !frame->Bitmap().isNull();
  if (!has_decoded_frame) {
    decode_failed_ = decoder->Failed();
    if (resume_decoding) {
      ImageDecodingStore::Instance().UnlockDecoder(generator_, client_id_,
                                                   decoder);
    }
    return false;
  }

  SkBitmap scaled_size_bitmap = frame->Bitmap();
  DCHECK_EQ(scaled_size_bitmap.width(), scaled_size_.width());
  DCHECK_EQ(scaled_size_bitmap.height(), scaled_size_.height());

  // If we decoded into external memory, the bitmap should be backed by the
  // pixels passed to the allocator.
  DCHECK(!decode_to_external_memory ||
         scaled_size_bitmap.getPixels() == pixels_);

  *has_alpha = !scaled_size_bitmap.isOpaque();
  if (!decode_to_external_memory)
    scaled_size_bitmap.readPixels(info_, pixels_, row_bytes_, 0, 0);

  // Free as much memory as possible.  For single-frame images, we can
  // just delete the decoder entirely if they use the external allocator.
  // For multi-frame images, we keep the decoder around in order to preserve
  // decoded information such as the required previous frame indexes, but if
  // we've reached the last frame we can at least delete all the cached frames.
  // (If we were to do this before reaching the last frame, any subsequent
  // requested frames which relied on the current frame would trigger extra
  // re-decoding of all frames in the dependency chain).
  const bool frame_was_completely_decoded =
      frame->GetStatus() == ImageFrame::kFrameComplete || all_data_received_;
  PurgeAllFramesIfNecessary(decoder, frame_was_completely_decoded,
                            *frame_count);

  const bool should_remove_decoder = ShouldRemoveDecoder(
      frame_was_completely_decoded, decode_to_external_memory);
  if (resume_decoding) {
    if (should_remove_decoder) {
      ImageDecodingStore::Instance().RemoveDecoder(generator_, client_id_,
                                                   decoder);
    } else {
      ImageDecodingStore::Instance().UnlockDecoder(generator_, client_id_,
                                                   decoder);
    }
  } else if (!should_remove_decoder) {
    // If we have a newly created decoder which we don't want to remove, add
    // it to the cache.
    ImageDecodingStore::Instance().InsertDecoder(generator_, client_id_,
                                                 std::move(new_decoder));
  }

  return true;
}

bool ImageDecoderWrapper::ShouldDecodeToExternalMemory(
    size_t frame_count,
    bool resume_decoding) const {
  // Multi-frame images need their decode cached in the decoder to allow using
  // subsequent frames to be decoded by caching dependent frames.
  // Also external allocators don't work for multi-frame images right now.
  if (generator_->IsMultiFrame())
    return false;

  // On low-end devices, always use the external allocator, to avoid storing
  // duplicate copies of the data for partial decodes in the ImageDecoder's
  // cache.
  if (Platform::Current()->IsLowEndDevice()) {
    DCHECK(!resume_decoding);
    return true;
  }

  // TODO (scroggo): If !is_multi_frame_ && new_decoder && frame_count_, it
  // should always be the case that 1u == frame_count_. But it looks like it
  // is currently possible for frame_count_ to be another value.
  if (1u == frame_count && all_data_received_ && !resume_decoding) {
    // Also use external allocator in situations when all of the data has been
    // received and there is not already a partial cache in the image decoder.
    return true;
  }

  return false;
}

bool ImageDecoderWrapper::ShouldRemoveDecoder(
    bool frame_was_completely_decoded,
    bool decoded_to_external_memory) const {
  // Mult-frame images need the decode cached to allow decoding subsequent
  // frames without having to decode the complete dependency chain. For this
  // reason, we should never be decoding directly to external memory for these
  // images.
  if (generator_->IsMultiFrame()) {
    DCHECK(!decoded_to_external_memory);
    return false;
  }

  // If the decode was done directly to external memory, the decoder has no
  // data to cache. Remove it.
  if (decoded_to_external_memory)
    return true;

  // If we were caching a decoder with a partially decoded frame which has
  // now been completely decoded, we don't need to cache this decoder anymore.
  if (frame_was_completely_decoded)
    return true;

  return false;
}

void ImageDecoderWrapper::PurgeAllFramesIfNecessary(
    ImageDecoder* decoder,
    bool frame_was_completely_decoded,
    size_t frame_count) const {
  // We only purge all frames when we have decoded the last frame for a
  // multi-frame image. This is because once the last frame is decoded, the
  // animation will loop back to the first frame which does not need the last
  // frame as a dependency and therefore can be purged.
  // For single-frame images, the complete decoder is removed once it has been
  // completely decoded.
  if (!generator_->IsMultiFrame())
    return;

  // The frame was only partially decoded, we need to retain it to be able to
  // resume the decoder.
  if (!frame_was_completely_decoded)
    return;

  const size_t last_frame_index = frame_count - 1;
  if (frame_index_ == last_frame_index)
    decoder->ClearCacheExceptFrame(kNotFound);
}

std::unique_ptr<ImageDecoder> ImageDecoderWrapper::CreateDecoderWithData(
    ImageDecoderFactory* factory) const {
  if (factory) {
    auto decoder = factory->Create();
    if (decoder)
      decoder->SetData(data_, all_data_received_);
    return decoder;
  }

  // The newly created decoder just grabbed the data.  No need to reset it.
  return ImageDecoder::Create(data_, all_data_received_, alpha_option_,
                              decoding_option_, decoder_color_behavior_,
                              ImageDecoder::OverrideAllowDecodeToYuv::kDeny,
                              scaled_size_);
}

}  // namespace blink
