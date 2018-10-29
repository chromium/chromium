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

#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/graphics/decoding_image_generator.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

struct DeferredFrameData {
  DISALLOW_NEW();

 public:
  DeferredFrameData()
      : orientation_(kDefaultImageOrientation),
        is_received_(false) {}

  ImageOrientation orientation_;
  TimeDelta duration_;
  bool is_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeferredFrameData);
};

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::Create(
    scoped_refptr<SharedBuffer> data,
    bool data_complete,
    ImageDecoder::AlphaOption alpha_option,
    const ColorBehavior& color_behavior) {
  std::unique_ptr<ImageDecoder> metadata_decoder =
      ImageDecoder::Create(data, data_complete, alpha_option,
                           ImageDecoder::kDefaultBitDepth, color_behavior);
  if (!metadata_decoder)
    return nullptr;

  std::unique_ptr<DeferredImageDecoder> decoder(
      new DeferredImageDecoder(std::move(metadata_decoder)));

  // Since we've just instantiated a fresh decoder, there's no need to reset its
  // data.
  decoder->SetDataInternal(std::move(data), data_complete, false);

  return decoder;
}

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::CreateForTesting(
    std::unique_ptr<ImageDecoder> metadata_decoder) {
  return base::WrapUnique(
      new DeferredImageDecoder(std::move(metadata_decoder)));
}

DeferredImageDecoder::DeferredImageDecoder(
    std::unique_ptr<ImageDecoder> metadata_decoder)
    : metadata_decoder_(std::move(metadata_decoder)),
      repetition_count_(kAnimationNone),
      all_data_received_(false),
      can_yuv_decode_(false),
      has_hot_spot_(false),
      image_is_high_bit_depth_(false),
      complete_frame_content_id_(PaintImage::GetNextContentId()) {}

DeferredImageDecoder::~DeferredImageDecoder() = default;

String DeferredImageDecoder::FilenameExtension() const {
  return metadata_decoder_ ? metadata_decoder_->FilenameExtension()
                           : filename_extension_;
}

sk_sp<PaintImageGenerator> DeferredImageDecoder::CreateGenerator(size_t index) {
  if (frame_generator_ && frame_generator_->DecodeFailed())
    return nullptr;

  PrepareLazyDecodedFrames();

  // PrepareLazyDecodedFrames should populate the metadata for each frame in
  // this image and create the |frame_generator_|, if enough data is available.
  if (index >= frame_data_.size())
    return nullptr;

  DCHECK(frame_generator_);
  const SkISize& decoded_size = frame_generator_->GetFullSize();
  DCHECK_GT(decoded_size.width(), 0);
  DCHECK_GT(decoded_size.height(), 0);

  sk_sp<SkROBuffer> ro_buffer(rw_buffer_->makeROBufferSnapshot());
  scoped_refptr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSkROBuffer(std::move(ro_buffer));

  // ImageFrameGenerator has the latest known alpha state. There will be a
  // performance boost if this frame is opaque.
  SkAlphaType alpha_type = frame_generator_->HasAlpha(index)
                               ? kPremul_SkAlphaType
                               : kOpaque_SkAlphaType;
  SkImageInfo info =
      SkImageInfo::MakeN32(decoded_size.width(), decoded_size.height(),
                           alpha_type, color_space_for_sk_images_);
  if (image_is_high_bit_depth_)
    info = info.makeColorType(kRGBA_F16_SkColorType);

  std::vector<FrameMetadata> frames(frame_data_.size());
  for (size_t i = 0; i < frame_data_.size(); ++i) {
    frames[i].complete = frame_data_[i].is_received_;
    frames[i].duration = FrameDurationAtIndex(i);
  }

  auto generator = DecodingImageGenerator::Create(
      frame_generator_, info, std::move(segment_reader), std::move(frames),
      complete_frame_content_id_, all_data_received_);
  generator->SetCanYUVDecode(can_yuv_decode_);

  return generator;
}

scoped_refptr<SharedBuffer> DeferredImageDecoder::Data() {
  if (!rw_buffer_)
    return nullptr;
  sk_sp<SkROBuffer> ro_buffer(rw_buffer_->makeROBufferSnapshot());
  scoped_refptr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  SkROBuffer::Iter it(ro_buffer.get());
  do {
    shared_buffer->Append(static_cast<const char*>(it.data()), it.size());
  } while (it.next());
  return shared_buffer;
}

void DeferredImageDecoder::SetData(scoped_refptr<SharedBuffer> data,
                                   bool all_data_received) {
  SetDataInternal(std::move(data), all_data_received, true);
}

void DeferredImageDecoder::SetDataInternal(scoped_refptr<SharedBuffer> data,
                                           bool all_data_received,
                                           bool push_data_to_decoder) {
  if (metadata_decoder_) {
    all_data_received_ = all_data_received;
    if (push_data_to_decoder)
      metadata_decoder_->SetData(data, all_data_received);
    PrepareLazyDecodedFrames();
  }

  if (frame_generator_) {
    if (!rw_buffer_)
      rw_buffer_ = std::make_unique<SkRWBuffer>(data->size());

    for (auto it = data->GetIteratorAt(rw_buffer_->size()); it != data->cend();
         ++it) {
      DCHECK_GE(data->size(), rw_buffer_->size() + it->size());
      const size_t remaining = data->size() - rw_buffer_->size() - it->size();
      rw_buffer_->append(it->data(), it->size(), remaining);
    }
  }
}

bool DeferredImageDecoder::IsSizeAvailable() {
  // m_actualDecoder is 0 only if image decoding is deferred and that means
  // the image header decoded successfully and the size is available.
  return metadata_decoder_ ? metadata_decoder_->IsSizeAvailable() : true;
}

bool DeferredImageDecoder::HasEmbeddedColorProfile() const {
  return metadata_decoder_ ? metadata_decoder_->HasEmbeddedColorProfile()
                           : has_embedded_color_profile_;
}

IntSize DeferredImageDecoder::Size() const {
  return metadata_decoder_ ? metadata_decoder_->Size() : size_;
}

IntSize DeferredImageDecoder::FrameSizeAtIndex(size_t index) const {
  // FIXME: LocalFrame size is assumed to be uniform. This might not be true for
  // future supported codecs.
  return metadata_decoder_ ? metadata_decoder_->FrameSizeAtIndex(index) : size_;
}

size_t DeferredImageDecoder::FrameCount() {
  return metadata_decoder_ ? metadata_decoder_->FrameCount()
                           : frame_data_.size();
}

int DeferredImageDecoder::RepetitionCount() const {
  return metadata_decoder_ ? metadata_decoder_->RepetitionCount()
                           : repetition_count_;
}

void DeferredImageDecoder::ClearCacheExceptFrame(size_t clear_except_frame) {
  if (metadata_decoder_)
    metadata_decoder_->ClearCacheExceptFrame(clear_except_frame);
}

bool DeferredImageDecoder::FrameHasAlphaAtIndex(size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->FrameHasAlphaAtIndex(index);
  if (!frame_generator_->IsMultiFrame())
    return frame_generator_->HasAlpha(index);
  return true;
}

bool DeferredImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->FrameIsReceivedAtIndex(index);
  if (index < frame_data_.size())
    return frame_data_[index].is_received_;
  return false;
}

TimeDelta DeferredImageDecoder::FrameDurationAtIndex(size_t index) const {
  TimeDelta duration;
  if (metadata_decoder_)
    duration = metadata_decoder_->FrameDurationAtIndex(index);
  if (index < frame_data_.size())
    duration = frame_data_[index].duration_;

  // Many annoying ads specify a 0 duration to make an image flash as quickly as
  // possible. We follow Firefox's behavior and use a duration of 100 ms for any
  // frames that specify a duration of <= 10 ms. See <rdar://problem/7689300>
  // and <http://webkit.org/b/36082> for more information.
  if (duration <= TimeDelta::FromMilliseconds(10))
    duration = TimeDelta::FromMilliseconds(100);

  return duration;
}

ImageOrientation DeferredImageDecoder::OrientationAtIndex(size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->Orientation();
  if (index < frame_data_.size())
    return frame_data_[index].orientation_;
  return kDefaultImageOrientation;
}

void DeferredImageDecoder::ActivateLazyDecoding() {
  if (frame_generator_)
    return;

  size_ = metadata_decoder_->Size();
  image_is_high_bit_depth_ = metadata_decoder_->ImageIsHighBitDepth();
  has_hot_spot_ = metadata_decoder_->HotSpot(hot_spot_);
  filename_extension_ = metadata_decoder_->FilenameExtension();
  // JPEG images support YUV decoding; other decoders do not. (WebP could in the
  // future.)
  can_yuv_decode_ = RuntimeEnabledFeatures::DecodeToYUVEnabled() &&
                    (filename_extension_ == "jpg");
  has_embedded_color_profile_ = metadata_decoder_->HasEmbeddedColorProfile();
  color_space_for_sk_images_ = metadata_decoder_->ColorSpaceForSkImages();

  const bool is_single_frame =
      metadata_decoder_->RepetitionCount() == kAnimationNone ||
      (all_data_received_ && metadata_decoder_->FrameCount() == 1u);
  const SkISize decoded_size =
      SkISize::Make(metadata_decoder_->DecodedSize().Width(),
                    metadata_decoder_->DecodedSize().Height());
  frame_generator_ = ImageFrameGenerator::Create(
      decoded_size, !is_single_frame, metadata_decoder_->GetColorBehavior(),
      metadata_decoder_->GetSupportedDecodeSizes());
}

void DeferredImageDecoder::PrepareLazyDecodedFrames() {
  if (!metadata_decoder_ || !metadata_decoder_->IsSizeAvailable())
    return;

  ActivateLazyDecoding();

  const size_t previous_size = frame_data_.size();
  frame_data_.resize(metadata_decoder_->FrameCount());

  // We have encountered a broken image file. Simply bail.
  if (frame_data_.size() < previous_size)
    return;

  for (size_t i = previous_size; i < frame_data_.size(); ++i) {
    frame_data_[i].duration_ = metadata_decoder_->FrameDurationAtIndex(i);
    frame_data_[i].orientation_ = metadata_decoder_->Orientation();
    frame_data_[i].is_received_ = metadata_decoder_->FrameIsReceivedAtIndex(i);
  }

  // The last lazy decoded frame created from previous call might be
  // incomplete so update its state.
  if (previous_size) {
    const size_t last_frame = previous_size - 1;
    frame_data_[last_frame].is_received_ =
        metadata_decoder_->FrameIsReceivedAtIndex(last_frame);
  }

  // If we've received all of the data, then we can reset the metadata decoder,
  // since everything we care about should now be stored in |frame_data_|.
  if (all_data_received_) {
    repetition_count_ = metadata_decoder_->RepetitionCount();
    metadata_decoder_.reset();
    // Hold on to m_rwBuffer, which is still needed by createFrameAtIndex.
  }
}

bool DeferredImageDecoder::HotSpot(IntPoint& hot_spot) const {
  if (metadata_decoder_)
    return metadata_decoder_->HotSpot(hot_spot);
  if (has_hot_spot_)
    hot_spot = hot_spot_;
  return has_hot_spot_;
}

}  // namespace blink

namespace WTF {
template <>
struct VectorTraits<blink::DeferredFrameData>
    : public SimpleClassVectorTraits<blink::DeferredFrameData> {
  STATIC_ONLY(VectorTraits);
  static const bool kCanInitializeWithMemset =
      false;  // Not all DeferredFrameData members initialize to 0.
};
}
