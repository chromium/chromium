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
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/deferred_image_decoder.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

const int kMinImageSizeForClassification1D = 24;
const int kMaxImageSizeForClassification1D = 100;

}  // namespace

int GetRepetitionCountWithPolicyOverride(int actual_count,
                                         ImageAnimationPolicy policy) {
  if (actual_count == kAnimationNone ||
      policy == kImageAnimationPolicyNoAnimation) {
    return kAnimationNone;
  }

  if (actual_count == kAnimationLoopOnce ||
      policy == kImageAnimationPolicyAnimateOnce) {
    return kAnimationLoopOnce;
  }

  return actual_count;
}

BitmapImage::BitmapImage(ImageObserver* observer, bool is_multipart)
    : Image(observer, is_multipart),
      animation_policy_(kImageAnimationPolicyAllowed),
      all_data_received_(false),
      have_size_(false),
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

void BitmapImage::NotifyMemoryChanged() {
  if (GetImageObserver())
    GetImageObserver()->DecodedSizeChangedTo(this, TotalFrameBytes());
}

size_t BitmapImage::TotalFrameBytes() {
  if (cached_frame_)
    return static_cast<size_t>(Size().Area()) * sizeof(ImageFrame::PixelData);
  return 0u;
}

PaintImage BitmapImage::PaintImageForTesting() {
  return CreatePaintImage();
}

PaintImage BitmapImage::CreatePaintImage() {
  sk_sp<PaintImageGenerator> generator =
      decoder_ ? decoder_->CreateGenerator(PaintImage::kDefaultFrameIndex)
               : nullptr;
  if (!generator)
    return PaintImage();

  auto completion_state = all_data_received_
                              ? PaintImage::CompletionState::DONE
                              : PaintImage::CompletionState::PARTIALLY_DONE;
  auto builder =
      CreatePaintImageBuilder()
          .set_paint_image_generator(std::move(generator))
          .set_repetition_count(GetRepetitionCountWithPolicyOverride(
              RepetitionCount(), animation_policy_))
          .set_is_high_bit_depth(decoder_->ImageIsHighBitDepth())
          .set_completion_state(completion_state)
          .set_reset_animation_sequence_id(reset_animation_sequence_id_);

  return builder.TakePaintImage();
}

void BitmapImage::UpdateSize() const {
  if (!size_available_ || have_size_ || !decoder_)
    return;

  size_ = decoder_->FrameSizeAtIndex(0);
  if (decoder_->OrientationAtIndex(0).UsesWidthAsHeight())
    size_respecting_orientation_ = size_.TransposedSize();
  else
    size_respecting_orientation_ = size_;
  have_size_ = true;
}

IntSize BitmapImage::Size() const {
  UpdateSize();
  return size_;
}

IntSize BitmapImage::SizeRespectingOrientation() const {
  UpdateSize();
  return size_respecting_orientation_;
}

bool BitmapImage::GetHotSpot(IntPoint& hot_spot) const {
  return decoder_ && decoder_->HotSpot(hot_spot);
}

// We likely don't need to confirm that this is the first time all data has
// been received as a way to avoid reporting the UMA multiple times for the
// same image. However, we err on the side of caution.
bool BitmapImage::ShouldReportByteSizeUMAs(bool data_now_completely_received) {
  if (!decoder_)
    return false;
  // Ensures that refactoring to check truthiness of ByteSize() method is
  // equivalent to the previous use of Data() and does not mess up UMAs.
  DCHECK_EQ(!decoder_->ByteSize(), !decoder_->Data());
  return !all_data_received_ && data_now_completely_received &&
         decoder_->ByteSize() && IsSizeAvailable();
}

Image::SizeAvailability BitmapImage::SetData(scoped_refptr<SharedBuffer> data,
                                             bool all_data_received) {
  if (!data)
    return kSizeAvailable;

  int length = data->size();
  if (!length)
    return kSizeAvailable;

  if (decoder_) {
    decoder_->SetData(std::move(data), all_data_received);
    return DataChanged(all_data_received);
  }

  bool has_enough_data = ImageDecoder::HasSufficientDataToSniffImageType(*data);
  decoder_ = DeferredImageDecoder::Create(std::move(data), all_data_received,
                                          ImageDecoder::kAlphaPremultiplied,
                                          ColorBehavior::Tag());
  // If we had enough data but couldn't create a decoder, it implies a decode
  // failure.
  if (has_enough_data && !decoder_)
    return kSizeAvailable;
  return DataChanged(all_data_received);
}

// Return the image density in 0.01 "bits per pixel" rounded to the nearest
// integer.
static inline uint64_t ImageDensityInCentiBpp(IntSize size,
                                              size_t image_size_bytes) {
  uint64_t image_area = static_cast<uint64_t>(size.Width()) * size.Height();
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
  if (ShouldReportByteSizeUMAs(all_data_received) &&
      decoder_->FilenameExtension() == "jpg") {
    BitmapImageMetrics::CountImageJpegDensity(
        std::min(Size().Width(), Size().Height()),
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

void BitmapImage::Draw(
    cc::PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    RespectImageOrientationEnum should_respect_image_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode decode_mode) {
  TRACE_EVENT0("skia", "BitmapImage::draw");

  PaintImage image = PaintImageForCurrentFrame();
  if (!image)
    return;  // It's too early and we don't have an image yet.

  auto paint_image_decoding_mode = ToPaintImageDecodingMode(decode_mode);
  if (image.decoding_mode() != paint_image_decoding_mode) {
    image = PaintImageBuilder::WithCopy(std::move(image))
                .set_decoding_mode(paint_image_decoding_mode)
                .TakePaintImage();
  }

  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::MakeWH(image.width(), image.height()));

  if (adjusted_src_rect.IsEmpty() || dst_rect.IsEmpty())
    return;  // Nothing to draw.

  ImageOrientation orientation = kDefaultImageOrientation;
  if (should_respect_image_orientation == kRespectImageOrientation)
    orientation = CurrentFrameOrientation();

  PaintCanvasAutoRestore auto_restore(canvas, false);
  FloatRect adjusted_dst_rect = dst_rect;
  if (orientation != kDefaultImageOrientation) {
    canvas->save();

    // ImageOrientation expects the origin to be at (0, 0)
    canvas->translate(adjusted_dst_rect.X(), adjusted_dst_rect.Y());
    adjusted_dst_rect.SetLocation(FloatPoint());

    canvas->concat(AffineTransformToSkMatrix(
        orientation.TransformFromDefault(adjusted_dst_rect.Size())));

    if (orientation.UsesWidthAsHeight()) {
      // The destination rect will have its width and height already reversed
      // for the orientation of the image, as it was needed for page layout, so
      // we need to reverse it back here.
      adjusted_dst_rect =
          FloatRect(adjusted_dst_rect.X(), adjusted_dst_rect.Y(),
                    adjusted_dst_rect.Height(), adjusted_dst_rect.Width());
    }
  }

  uint32_t unique_id = image.GetSkImage()->uniqueID();
  bool is_lazy_generated = image.IsLazyGenerated();
  canvas->drawImageRect(std::move(image), adjusted_src_rect, adjusted_dst_rect,
                        &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));

  if (is_lazy_generated) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "Draw LazyPixelRef", TRACE_EVENT_SCOPE_THREAD,
                         "LazyPixelRef", unique_id);
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

static inline bool HasVisibleImageSize(IntSize size) {
  return (size.Width() > 1 || size.Height() > 1);
}

bool BitmapImage::IsSizeAvailable() {
  if (size_available_)
    return true;

  size_available_ = decoder_ && decoder_->IsSizeAvailable();
  if (size_available_ && HasVisibleImageSize(Size())) {
    BitmapImageMetrics::CountDecodedImageType(decoder_->FilenameExtension());
    if (decoder_->FilenameExtension() == "jpg") {
      BitmapImageMetrics::CountImageOrientation(
          decoder_->OrientationAtIndex(0).Orientation());
    }
  }

  return size_available_;
}

PaintImage BitmapImage::PaintImageForCurrentFrame() {
  if (cached_frame_)
    return cached_frame_;

  cached_frame_ = CreatePaintImage();

  // Create the SkImage backing for this PaintImage here to ensure that copies
  // of the PaintImage share the same SkImage. Skia's caching of the decoded
  // output of this image is tied to the lifetime of the SkImage. So we create
  // the SkImage here and cache the PaintImage to keep the decode alive in
  // skia's cache.
  cached_frame_.GetSkImage();
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
  // If the image is animated, it is being animated by the compositor and we
  // don't know what the current frame is.
  // TODO(khushalsagar): We could say the image is opaque if none of the frames
  // have alpha.
  if (MaybeAnimated())
    return false;

  // We ask the decoder whether the image has alpha because in some cases the
  // the correct value is known after decoding. The DeferredImageDecoder caches
  // the accurate value from the decoded result.
  const bool frame_has_alpha =
      decoder_ ? decoder_->FrameHasAlphaAtIndex(PaintImage::kDefaultFrameIndex)
               : true;
  return !frame_has_alpha;
}

bool BitmapImage::CurrentFrameIsComplete() {
  return decoder_
             ? decoder_->FrameIsReceivedAtIndex(PaintImage::kDefaultFrameIndex)
             : false;
}

bool BitmapImage::CurrentFrameIsLazyDecoded() {
  // BitmapImage supports only lazy generated images.
  return true;
}

ImageOrientation BitmapImage::CurrentFrameOrientation() const {
  return decoder_ ? decoder_->OrientationAtIndex(PaintImage::kDefaultFrameIndex)
                  : kDefaultImageOrientation;
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

void BitmapImage::SetAnimationPolicy(ImageAnimationPolicy policy) {
  if (animation_policy_ == policy)
    return;

  animation_policy_ = policy;
  ResetAnimation();
}

DarkModeClassification BitmapImage::CheckTypeSpecificConditionsForDarkMode(
    const FloatRect& dest_rect,
    DarkModeImageClassifier* classifier) {
  if (dest_rect.Width() < kMinImageSizeForClassification1D ||
      dest_rect.Height() < kMinImageSizeForClassification1D)
    return DarkModeClassification::kApplyFilter;

  if (dest_rect.Width() > kMaxImageSizeForClassification1D ||
      dest_rect.Height() > kMaxImageSizeForClassification1D) {
    return DarkModeClassification::kDoNotApplyFilter;
  }

  classifier->SetImageType(DarkModeImageClassifier::ImageType::kBitmap);

  return DarkModeClassification::kNotClassified;
}

}  // namespace blink
