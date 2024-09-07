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
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/decoding_image_generator.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

struct DeferredFrameData {
  DISALLOW_NEW();

 public:
  DeferredFrameData()
      : orientation_(ImageOrientationEnum::kDefault), is_received_(false) {}
  DeferredFrameData(const DeferredFrameData&) = delete;
  DeferredFrameData& operator=(const DeferredFrameData&) = delete;

  ImageOrientation orientation_;
  gfx::Size density_corrected_size_;
  base::TimeDelta duration_;
  bool is_received_;
};

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::Create(
    scoped_refptr<SharedBuffer> data,
    bool data_complete,
    ImageDecoder::AlphaOption alpha_option,
    ColorBehavior color_behavior) {
  std::unique_ptr<ImageDecoder> metadata_decoder = ImageDecoder::Create(
      data, data_complete, alpha_option, ImageDecoder::kDefaultBitDepth,
      color_behavior, cc::AuxImage::kDefault,
      Platform::GetMaxDecodedImageBytes());
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
      first_decoding_generator_created_(false),
      can_yuv_decode_(false),
      has_hot_spot_(false),
      image_is_high_bit_depth_(false),
      complete_frame_content_id_(PaintImage::GetNextContentId()) {
}

DeferredImageDecoder::~DeferredImageDecoder() {
}

String DeferredImageDecoder::FilenameExtension() const {
  return metadata_decoder_ ? metadata_decoder_->FilenameExtension()
                           : filename_extension_;
}

const AtomicString& DeferredImageDecoder::MimeType() const {
  return metadata_decoder_ ? metadata_decoder_->MimeType() : mime_type_;
}

sk_sp<PaintImageGenerator> DeferredImageDecoder::CreateGenerator() {
  if (frame_generator_ && frame_generator_->DecodeFailed())
    return nullptr;

  if (invalid_image_ || frame_data_.empty())
    return nullptr;

  DCHECK(frame_generator_);
  const SkISize& decoded_size = frame_generator_->GetFullSize();
  DCHECK_GT(decoded_size.width(), 0);
  DCHECK_GT(decoded_size.height(), 0);

  scoped_refptr<SegmentReader> segment_reader =
      parkable_image_->MakeROSnapshot();

  SkImageInfo info =
      SkImageInfo::MakeN32(decoded_size.width(), decoded_size.height(),
                           AlphaType(), color_space_for_sk_images_);
  if (image_is_high_bit_depth_)
    info = info.makeColorType(kRGBA_F16_SkColorType);

  WebVector<FrameMetadata> frames(frame_data_.size());
  for (wtf_size_t i = 0; i < frame_data_.size(); ++i) {
    frames[i].complete = frame_data_[i].is_received_;
    frames[i].duration = FrameDurationAtIndex(i);
  }

  if (!first_decoding_generator_created_) {
    DCHECK(!incremental_decode_needed_.has_value());
    incremental_decode_needed_ = !all_data_received_;
  }
  DCHECK(incremental_decode_needed_.has_value());

  // TODO(crbug.com/943519):
  // If we haven't received all data, we might veto YUV and begin doing
  // incremental RGB decoding until all data were received. Then the final
  // decode would be in YUV (but from the beginning of the image).
  //
  // The memory/speed tradeoffs of mixing RGB and YUV decoding are unclear due
  // to caching at various levels. Additionally, incremental decoding is less
  // common, so we avoid worrying about this with the line below.
  can_yuv_decode_ &= !incremental_decode_needed_.value();

  DCHECK(image_metadata_);
  image_metadata_->all_data_received_prior_to_decode =
      !incremental_decode_needed_.value();

  auto generator = DecodingImageGenerator::Create(
      frame_generator_, info, std::move(segment_reader), std::move(frames),
      complete_frame_content_id_, all_data_received_, can_yuv_decode_,
      *image_metadata_);
  first_decoding_generator_created_ = true;

  return generator;
}

bool DeferredImageDecoder::CreateGainmapGenerator(
    sk_sp<PaintImageGenerator>& gainmap_generator,
    SkGainmapInfo& gainmap_info) {
  if (!gainmap_) {
    return false;
  }
  WebVector<FrameMetadata> frames;

  SkImageInfo gainmap_image_info =
      SkImageInfo::Make(gainmap_->frame_generator->GetFullSize(),
                        kN32_SkColorType, kOpaque_SkAlphaType);
  gainmap_generator = DecodingImageGenerator::Create(
      gainmap_->frame_generator, gainmap_image_info, gainmap_->data, frames,
      complete_frame_content_id_, all_data_received_, gainmap_->can_decode_yuv,
      gainmap_->image_metadata);
  gainmap_info = gainmap_->info;
  return true;
}

scoped_refptr<SharedBuffer> DeferredImageDecoder::Data() {
  return parkable_image_ ? parkable_image_->Data() : nullptr;
}

bool DeferredImageDecoder::HasData() const {
  return parkable_image_ != nullptr;
}

size_t DeferredImageDecoder::DataSize() const {
  DCHECK(parkable_image_);
  return parkable_image_->size();
}

void DeferredImageDecoder::SetData(scoped_refptr<SharedBuffer> data,
                                   bool all_data_received) {
  SetDataInternal(std::move(data), all_data_received, true);
}

void DeferredImageDecoder::SetDataInternal(scoped_refptr<SharedBuffer> data,
                                           bool all_data_received,
                                           bool push_data_to_decoder) {
  // Once all the data has been received, the image should not change.
  DCHECK(!all_data_received_);
  if (metadata_decoder_) {
    all_data_received_ = all_data_received;
    if (push_data_to_decoder)
      metadata_decoder_->SetData(data, all_data_received);
    PrepareLazyDecodedFrames();
  }

  if (frame_generator_) {
    if (!parkable_image_)
      parkable_image_ = ParkableImage::Create(data->size());

    parkable_image_->Append(data.get(), parkable_image_->size());
  }

  if (all_data_received && parkable_image_)
    parkable_image_->Freeze();
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

gfx::Size DeferredImageDecoder::Size() const {
  return metadata_decoder_ ? metadata_decoder_->Size() : size_;
}

gfx::Size DeferredImageDecoder::FrameSizeAtIndex(wtf_size_t index) const {
  // FIXME: LocalFrame size is assumed to be uniform. This might not be true for
  // future supported codecs.
  return metadata_decoder_ ? metadata_decoder_->FrameSizeAtIndex(index) : size_;
}

wtf_size_t DeferredImageDecoder::FrameCount() {
  return metadata_decoder_ ? metadata_decoder_->FrameCount()
                           : frame_data_.size();
}

int DeferredImageDecoder::RepetitionCount() const {
  return metadata_decoder_ ? metadata_decoder_->RepetitionCount()
                           : repetition_count_;
}

SkAlphaType DeferredImageDecoder::AlphaType() const {
  // ImageFrameGenerator has the latest known alpha state. There will be a
  // performance boost if the image is opaque since we can avoid painting
  // the background in this case.
  // For multi-frame images, these maybe animated on the compositor thread.
  // So we can not mark them as opaque unless all frames are opaque.
  // TODO(khushalsagar): Check whether all frames being added to the
  // generator are opaque when populating FrameMetadata below.
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  if (frame_data_.size() == 1u && !frame_generator_->HasAlpha(0u))
    alpha_type = kOpaque_SkAlphaType;
  return alpha_type;
}

bool DeferredImageDecoder::FrameIsReceivedAtIndex(wtf_size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->FrameIsReceivedAtIndex(index);
  if (index < frame_data_.size())
    return frame_data_[index].is_received_;
  return false;
}

base::TimeDelta DeferredImageDecoder::FrameDurationAtIndex(
    wtf_size_t index) const {
  base::TimeDelta duration;
  if (metadata_decoder_)
    duration = metadata_decoder_->FrameDurationAtIndex(index);
  if (index < frame_data_.size())
    duration = frame_data_[index].duration_;

  // Many annoying ads specify a 0 duration to make an image flash as quickly as
  // possible. We follow Firefox's behavior and use a duration of 100 ms for any
  // frames that specify a duration of <= 10 ms. See <rdar://problem/7689300>
  // and <http://webkit.org/b/36082> for more information.
  if (duration <= base::Milliseconds(10))
    duration = base::Milliseconds(100);

  return duration;
}

ImageOrientation DeferredImageDecoder::OrientationAtIndex(
    wtf_size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->Orientation();
  if (index < frame_data_.size())
    return frame_data_[index].orientation_;
  return ImageOrientationEnum::kDefault;
}

gfx::Size DeferredImageDecoder::DensityCorrectedSizeAtIndex(
    wtf_size_t index) const {
  if (metadata_decoder_)
    return metadata_decoder_->DensityCorrectedSize();
  if (index < frame_data_.size())
    return frame_data_[index].density_corrected_size_;
  return Size();
}

size_t DeferredImageDecoder::ByteSize() const {
  return parkable_image_ ? parkable_image_->size() : 0u;
}

void DeferredImageDecoder::ActivateLazyDecoding() {
  ActivateLazyGainmapDecoding();
  if (frame_generator_)
    return;

  size_ = metadata_decoder_->Size();
  image_is_high_bit_depth_ = metadata_decoder_->ImageIsHighBitDepth();
  has_hot_spot_ = metadata_decoder_->HotSpot(hot_spot_);
  filename_extension_ = metadata_decoder_->FilenameExtension();
  mime_type_ = metadata_decoder_->MimeType();
  has_embedded_color_profile_ = metadata_decoder_->HasEmbeddedColorProfile();
  color_space_for_sk_images_ = metadata_decoder_->ColorSpaceForSkImages();

  const bool is_single_frame =
      metadata_decoder_->RepetitionCount() == kAnimationNone ||
      (all_data_received_ && metadata_decoder_->FrameCount() == 1u);
  const SkISize decoded_size =
      gfx::SizeToSkISize(metadata_decoder_->DecodedSize());
  frame_generator_ = ImageFrameGenerator::Create(
      decoded_size, !is_single_frame, metadata_decoder_->GetColorBehavior(),
      cc::AuxImage::kDefault, metadata_decoder_->GetSupportedDecodeSizes());
}

void DeferredImageDecoder::ActivateLazyGainmapDecoding() {
  // Early-out if we have excluded the possibility that this image has a
  // gainmap, or if we have already created the gainmap frame generator.
  if (!might_have_gainmap_ || gainmap_) {
    return;
  }

  // Do not decode gainmaps until all data is received (spatially incrementally
  // adding HDR to an image looks odd).
  if (!all_data_received_) {
    return;
  }

  // Attempt to extract the gainmap's data.
  std::unique_ptr<Gainmap> gainmap(new Gainmap);
  if (!metadata_decoder_->GetGainmapInfoAndData(gainmap->info, gainmap->data)) {
    might_have_gainmap_ = false;
    return;
  }
  DCHECK(gainmap->data);

  // Extract metadata from the gainmap's data.
  auto gainmap_metadata_decoder = ImageDecoder::Create(
      gainmap->data, all_data_received_, ImageDecoder::kAlphaNotPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::kIgnore,
      cc::AuxImage::kGainmap, Platform::GetMaxDecodedImageBytes());
  if (!gainmap_metadata_decoder) {
    DLOG(ERROR) << "Failed to create gainmap image decoder.";
    might_have_gainmap_ = false;
    return;
  }

  // Animated gainmap support does not exist.
  if (gainmap_metadata_decoder->FrameCount() != 1) {
    DLOG(ERROR) << "Animated gainmap images are not supported.";
    might_have_gainmap_ = false;
    return;
  }
  const bool kIsMultiFrame = false;

  // Create the result frame generator and metadata.
  gainmap->frame_generator = ImageFrameGenerator::Create(
      gfx::SizeToSkISize(gainmap_metadata_decoder->DecodedSize()),
      kIsMultiFrame, ColorBehavior::kIgnore, cc::AuxImage::kGainmap,
      gainmap_metadata_decoder->GetSupportedDecodeSizes());

  // Populate metadata and save to the `gainmap_` member.
  gainmap->can_decode_yuv = gainmap_metadata_decoder->CanDecodeToYUV();
  gainmap->image_metadata =
      gainmap_metadata_decoder->MakeMetadataForDecodeAcceleration();
  gainmap_ = std::move(gainmap);
}

void DeferredImageDecoder::PrepareLazyDecodedFrames() {
  if (!metadata_decoder_ || !metadata_decoder_->IsSizeAvailable())
    return;

  if (invalid_image_)
    return;

  if (!image_metadata_)
    image_metadata_ = metadata_decoder_->MakeMetadataForDecodeAcceleration();

  // If the image contains a coded size with zero in either or both size
  // dimensions, the image is invalid.
  if (image_metadata_->coded_size.has_value() &&
      image_metadata_->coded_size.value().IsEmpty()) {
    invalid_image_ = true;
    return;
  }

  ActivateLazyDecoding();

  const wtf_size_t previous_size = frame_data_.size();
  frame_data_.resize(metadata_decoder_->FrameCount());

  // The decoder may be invalidated during a FrameCount(). Simply bail if so.
  if (metadata_decoder_->Failed()) {
    invalid_image_ = true;
    return;
  }

  // We have encountered a broken image file. Simply bail.
  if (frame_data_.size() < previous_size) {
    invalid_image_ = true;
    return;
  }

  for (wtf_size_t i = previous_size; i < frame_data_.size(); ++i) {
    frame_data_[i].duration_ = metadata_decoder_->FrameDurationAtIndex(i);
    frame_data_[i].orientation_ = metadata_decoder_->Orientation();
    frame_data_[i].density_corrected_size_ =
        metadata_decoder_->DensityCorrectedSize();
  }

  // Update the is_received_ state of incomplete frames.
  while (received_frame_count_ < frame_data_.size() &&
         metadata_decoder_->FrameIsReceivedAtIndex(received_frame_count_)) {
    frame_data_[received_frame_count_++].is_received_ = true;
  }

  can_yuv_decode_ =
      metadata_decoder_->CanDecodeToYUV() && all_data_received_ &&
      !frame_generator_->IsMultiFrame();

  // If we've received all of the data, then we can reset the metadata decoder,
  // since everything we care about should now be stored in |frame_data_|.
  if (all_data_received_) {
    repetition_count_ = metadata_decoder_->RepetitionCount();
    metadata_decoder_.reset();
    // Hold on to m_rwBuffer, which is still needed by createFrameAtIndex.
  }
}

bool DeferredImageDecoder::HotSpot(gfx::Point& hot_spot) const {
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
}  // namespace WTF
