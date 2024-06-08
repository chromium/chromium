/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

int GetRepetitionCountWithPolicyOverride(
    int actual_count,
    mojom::blink::ImageAnimationPolicy policy) {
  if (actual_count == kAnimationNone ||
      policy == mojom::blink::ImageAnimationPolicy::
                    kImageAnimationPolicyNoAnimation) {
    return kAnimationNone;
  }

  if (actual_count == kAnimationLoopOnce ||
      policy == mojom::blink::ImageAnimationPolicy::
                    kImageAnimationPolicyAnimateOnce) {
    return kAnimationLoopOnce;
  }

  return actual_count;
}

BitmapImage::BitmapImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      animation_policy_(
          mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAllowed),
      all_data_received_(false),
      have_size_(false),
      preferred_size_is_transposed_(false),
      size_available_(false),
      have_frame_count_(false),
      repetition_count_status_(kUnknown),
      repetition_count_(kAnimationNone),
      frame_count_(0) {}

BitmapImage::~BitmapImage() {}

bool BitmapImage::CurrentFrameHasSingleSecurityOrigin() const {
  return true;
}

void BitmapImage::DestroyDecodedData() {
  cached_frame_ = PaintImage();
  NotifyMemoryChanged();
}

scoped_refptr<SharedBuffer> BitmapImage::Data() {
  return decoder_ ? decoder_->Data() : nullptr;
}

bool BitmapImage::HasData() const {
  return decoder_ ? decoder_->HasData() : false;
}

size_t BitmapImage::DataSize() const {
  DCHECK(decoder_);
  return decoder_->DataSize();
}

void BitmapImage::NotifyMemoryChanged() {
  if (GetImageObserver())
    GetImageObserver()->DecodedSizeChangedTo(this, TotalFrameBytes());
}

size_t BitmapImage::TotalFrameBytes() {
  if (cached_frame_)
    return ClampTo<size_t>(Size().Area64() * sizeof(ImageFrame::PixelData));
  return 0u;
}

PaintImage BitmapImage::PaintImageForTesting() {
  return CreatePaintImage();
}

PaintImage BitmapImage::CreatePaintImage() {
  sk_sp<PaintImageGenerator> generator =
      decoder_ ? decoder_->CreateGenerator() : nullptr;
  if (!generator)
    return PaintImage();

  auto completion_state = all_data_received_
                              ? PaintImage::CompletionState::kDone
                              : PaintImage::CompletionState::kPartiallyDone;
  auto builder =
      CreatePaintImageBuilder()
          .set_paint_image_generator(std::move(generator))
          .set_repetition_count(GetRepetitionCountWithPolicyOverride(
              RepetitionCount(), animation_policy_))
          .set_is_high_bit_depth(decoder_->ImageIsHighBitDepth())
          .set_completion_state(completion_state)
          .set_reset_animation_sequence_id(reset_animation_sequence_id_);

  sk_sp<PaintImageGenerator> gainmap_generator;
  SkGainmapInfo gainmap_info;
  if (decoder_->CreateGainmapGenerator(gainmap_generator, gainmap_info)) {
    DCHECK(gainmap_generator);
    builder = builder.set_gainmap_paint_image_generator(
        std::move(gainmap_generator), gainmap_info);
  }

  return builder.TakePaintImage();
}

void BitmapImage::UpdateSize() const {
  if (have_size_ || !size_available_ || !decoder_)
    return;
  size_ = decoder_->FrameSizeAtIndex(0);
  density_corrected_size_ = decoder_->DensityCorrectedSizeAtIndex(0);
  preferred_size_is_transposed_ =
      decoder_->OrientationAtIndex(0).UsesWidthAsHeight();
  have_size_ = true;
}

gfx::Size BitmapImage::SizeWithConfig(SizeConfig config) const {
  UpdateSize();
  gfx::Size size = size_;
  if (config.apply_density && !density_corrected_size_.IsEmpty())
    size = density_corrected_size_;
  if (config.apply_orientation && preferred_size_is_transposed_)
    return gfx::TransposeSize(size);
  return size;
}

void BitmapImage::RecordDecodedImageType(UseCounter* use_counter) {
  BitmapImageMetrics::CountDecodedImageType(decoder_->FilenameExtension(),
                                            use_counter);
}

bool BitmapImage::GetHotSpot(gfx::Point& hot_spot) const {
  return decoder_ && decoder_->HotSpot(hot_spot);
}

// We likely don't need to confirm that this is the first time all data has
// been received as a way to avoid reporting the UMA multiple times for the
// same image. However, we err on the side of caution.
bool BitmapImage::ShouldReportByteSizeUMAs(bool data_now_completely_received) {
  if (!decoder_)
    return false;
  return !all_data_received_ && data_now_completely_received &&
         decoder_->ByteSize() != 0 && IsSizeAvailable() &&
         decoder_->RepetitionCount() == kAnimationNone &&
         !decoder_->ImageIsHighBitDepth();
}

Image::SizeAvailability BitmapImage::SetData(scoped_refptr<SharedBuffer> data,
                                             bool all_data_received) {
  if (!data)
    return kSizeAvailable;

  size_t length = data->size();
  if (!length)
    return kSizeAvailable;

  if (decoder_) {
    decoder_->SetData(std::move(data), all_data_received);
    return DataChanged(all_data_received);
  }

  bool has_enough_data = ImageDecoder::HasSufficientDataToSniffMimeType(*data);
  decoder_ = DeferredImageDecoder::Create(std::move(data), all_data_received,
                                          ImageDecoder::kAlphaPremultiplied,
                                          ColorBehavior::kTag);
  // If we had enough data but couldn't create a decoder, it implies a decode
  // failure.
  if (has_enough_data && !decoder_)
    return kSizeAvailable;
  return DataChanged(all_data_received);
}

// Return the image density in 0.01 "bits per pixel" rounded to the nearest
// integer.
static inline uint64_t ImageDensityInCentiBpp(gfx::Size size,
                                              size_t image_size_bytes) {
  uint64_t image_area = size.Area64();
  return (static_cast<uint64_t>(image_size_bytes) * 100 * 8 + image_area / 2) /
         image_area;
}

Image::SizeAvailability BitmapImage::DataChanged(bool all_data_received) {
  TRACE_EVENT0("blink", "BitmapImage::dataChanged");

  // If the data was updated, clear the |cached_frame_| to push it to the
  // compositor thread. Its necessary to clear the frame since more data
  // requires a new PaintImageGenerator instance.
  cached_frame_ = PaintImage();

  // Report the image density metric right after we received all the data. The
  // SetData() call on the decoder_ (if there is one) should have decoded the
  // images and we should know the image size at this point.
  if (ShouldReportByteSizeUMAs(all_data_received)) {
    BitmapImageMetrics::CountDecodedImageDensity(
        decoder_->FilenameExtension(),
        std::min(Size().width(), Size().height()),
        ImageDensityInCentiBpp(Size(), decoder_->ByteSize()),
        decoder_->ByteSize());
  }

  // Feed all the data we've seen so far to the image decoder.
  all_data_received_ = all_data_received;
  have_frame_count_ = false;

  return IsSizeAvailable() ? kSizeAvailable : kSizeUnavailable;
}

bool BitmapImage::HasColorProfile() const {
  return decoder_ && decoder_->HasEmbeddedColorProfile();
}

String BitmapImage::FilenameExtension() const {
  return decoder_ ? decoder_->FilenameExtension() : String();
}

const AtomicString& BitmapImage::MimeType() const {
  return decoder_ ? decoder_->MimeType() : g_null_atom;
}

void BitmapImage::Draw(cc::PaintCanvas* canvas,
                       const cc::PaintFlags& flags,
                       const gfx::RectF& dst_rect,
                       const gfx::RectF& src_rect,
                       const ImageDrawOptions& draw_options) {
  TRACE_EVENT0("skia", "BitmapImage::draw");

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // It's too early and we don't have an image yet.

  auto paint_image_decoding_mode =
      ToPaintImageDecodingMode(draw_options.decode_mode);
  if (image.decoding_mode() != paint_image_decoding_mode ||
      image.may_be_lcp_candidate() != draw_options.may_be_lcp_candidate) {
    image = PaintImageBuilder::WithCopy(std::move(image))
                .set_decoding_mode(paint_image_decoding_mode)
                .set_may_be_lcp_candidate(draw_options.may_be_lcp_candidate)
                .TakePaintImage();
  }

  gfx::RectF adjusted_src_rect = src_rect;
  if (!density_corrected_size_.IsEmpty()) {
    adjusted_src_rect.Scale(
        static_cast<float>(size_.width()) / density_corrected_size_.width(),
        static_cast<float>(size_.height()) / density_corrected_size_.height());
  }

  adjusted_src_rect.Intersect(gfx::RectF(image.width(), image.height()));

  if (adjusted_src_rect.IsEmpty() || dst_rect.IsEmpty())
    return;  // Nothing to draw.

  ImageOrientation orientation = ImageOrientationEnum::kDefault;
  if (draw_options.respect_orientation == kRespectImageOrientation)
    orientation = CurrentFrameOrientation();

  PaintCanvasAutoRestore auto_restore(canvas, false);
  gfx::RectF adjusted_dst_rect = dst_rect;
  if (orientation != ImageOrientationEnum::kDefault) {
    canvas->save();

    // ImageOrientation expects the origin to be at (0, 0)
    canvas->translate(adjusted_dst_rect.x(), adjusted_dst_rect.y());
    adjusted_dst_rect.set_origin(gfx::PointF());

    canvas->concat(AffineTransformToSkM44(
        orientation.TransformFromDefault(adjusted_dst_rect.size())));

    if (orientation.UsesWidthAsHeight()) {
      // The destination rect will have its width and height already reversed
      // for the orientation of the image, as it was needed for page layout, so
      // we need to reverse it back here.
      adjusted_dst_rect.set_size(gfx::TransposeSize(adjusted_dst_rect.size()));
    }
  }

  uint32_t stable_id = image.stable_id();
  bool is_lazy_generated = image.IsLazyGenerated();

  const cc::PaintFlags* image_flags = &flags;
  std::optional<cc::PaintFlags> dark_mode_flags;
  if (draw_options.dark_mode_filter) {
    dark_mode_flags = flags;
    draw_options.dark_mode_filter->ApplyFilterToImage(
        this, &dark_mode_flags.value(), gfx::RectFToSkRect(src_rect));
    image_flags = &dark_mode_flags.value();
  }
  canvas->drawImageRect(
      std::move(image), gfx::RectFToSkRect(adjusted_src_rect),
      gfx::RectFToSkRect(adjusted_dst_rect), draw_options.sampling_options,
      image_flags,
      WebCoreClampingModeToSkiaRectConstraint(draw_options.clamping_mode));

  if (is_lazy_generated) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "Draw LazyPixelRef", TRACE_EVENT_SCOPE_THREAD,
                         "LazyPixelRef", stable_id);
  }

  StartAnimation();
}

size_t BitmapImage::FrameCount() {
  if (!have_frame_count_) {
    frame_count_ = decoder_ ? decoder_->FrameCount() : 0;
    have_frame_count_ = frame_count_ > 0;
  }
  return frame_count_;
}

static inline bool HasVisibleImageSize(gfx::Size size) {
  return (size.width() > 1 || size.height() > 1);
}

bool BitmapImage::IsSizeAvailable() {
  if (size_available_)
    return true;

  size_available_ = decoder_ && decoder_->IsSizeAvailable();
  if (size_available_ && HasVisibleImageSize(Size()))
    BitmapImageMetrics::CountDecodedImageType(decoder_->FilenameExtension());

  return size_available_;
}

PaintImage BitmapImage::PaintImageForCurrentFrame() {
  auto alpha_type = decoder_ ? decoder_->AlphaType() : kUnknown_SkAlphaType;
  if (cached_frame_ && cached_frame_.GetAlphaType() == alpha_type)
    return cached_frame_;

  cached_frame_ = CreatePaintImage();

  // BitmapImage should not be texture backed.
  DCHECK(!cached_frame_.IsTextureBacked());

  // Create the SkImage backing for this PaintImage here to ensure that copies
  // of the PaintImage share the same SkImage. Skia's caching of the decoded
  // output of this image is tied to the lifetime of the SkImage. So we create
  // the SkImage here and cache the PaintImage to keep the decode alive in
  // skia's cache.
  cached_frame_.GetSwSkImage();
  NotifyMemoryChanged();

  return cached_frame_;
}

scoped_refptr<Image> BitmapImage::ImageForDefaultFrame() {
  if (FrameCount() > 1) {
    PaintImage paint_image = PaintImageForCurrentFrame();
    if (!paint_image)
      return nullptr;

    if (paint_image.ShouldAnimate()) {
      // To prevent the compositor from animating this image, we set the
      // animation count to kAnimationNone. This makes the image essentially
      // static.
      paint_image = PaintImageBuilder::WithCopy(std::move(paint_image))
                        .set_repetition_count(kAnimationNone)
                        .TakePaintImage();
    }
    return StaticBitmapImage::Create(std::move(paint_image));
  }

  return Image::ImageForDefaultFrame();
}

bool BitmapImage::CurrentFrameKnownToBeOpaque() {
  return decoder_ ? decoder_->AlphaType() == kOpaque_SkAlphaType : false;
}

bool BitmapImage::CurrentFrameIsComplete() {
  return decoder_ && decoder_->FrameIsReceivedAtIndex(0);
}

bool BitmapImage::CurrentFrameIsLazyDecoded() {
  // BitmapImage supports only lazy generated images.
  return true;
}

ImageOrientation BitmapImage::CurrentFrameOrientation() const {
  return decoder_ ? decoder_->OrientationAtIndex(0)
                  : ImageOrientationEnum::kDefault;
}

int BitmapImage::RepetitionCount() {
  if ((repetition_count_status_ == kUnknown) ||
      ((repetition_count_status_ == kUncertain) && all_data_received_)) {
    // Snag the repetition count.  If |imageKnownToBeComplete| is false, the
    // repetition count may not be accurate yet for GIFs; in this case the
    // decoder will default to cAnimationLoopOnce, and we'll try and read
    // the count again once the whole image is decoded.
    repetition_count_ = decoder_ ? decoder_->RepetitionCount() : kAnimationNone;

    // When requesting more than a single loop, repetition count is one less
    // than the actual number of loops.
    if (repetition_count_ > 0)
      repetition_count_++;

    repetition_count_status_ =
        (all_data_received_ || repetition_count_ == kAnimationNone)
            ? kCertain
            : kUncertain;
  }
  return repetition_count_;
}

void BitmapImage::ResetAnimation() {
  cached_frame_ = PaintImage();
  reset_animation_sequence_id_++;
}

bool BitmapImage::MaybeAnimated() {
  if (FrameCount() > 1)
    return true;

  return decoder_ && decoder_->RepetitionCount() != kAnimationNone;
}

void BitmapImage::SetAnimationPolicy(
    mojom::blink::ImageAnimationPolicy policy) {
  if (animation_policy_ == policy)
    return;

  animation_policy_ = policy;
  ResetAnimation();
}

}  // namespace blink
