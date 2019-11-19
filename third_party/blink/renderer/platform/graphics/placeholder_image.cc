// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"

#include <utility>

#include "base/stl_util.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace blink {

namespace {

// Placeholder image visual specifications:
// https://docs.google.com/document/d/1BHeA1azbgCdZgCnr16VN2g7A9MHPQ_dwKn5szh8evMQ/edit

constexpr int kIconWidth = 24;
constexpr int kIconHeight = 24;
constexpr int kFeaturePaddingX = 8;
constexpr int kIconPaddingY = 5;
constexpr int kPaddingBetweenIconAndText = 2;
constexpr int kTextPaddingY = 9;

constexpr int kFontSize = 14;

void DrawIcon(cc::PaintCanvas* canvas,
              const PaintFlags& flags,
              float x,
              float y,
              float scale_factor) {
  // Note that |icon_image| will be a 0x0 image when running
  // blink_platform_unittests.
  DEFINE_STATIC_REF(Image, icon_image,
                    (Image::LoadPlatformResource(IDR_PLACEHOLDER_ICON)));

  // Note that the |icon_image| is not scaled according to dest_rect / src_rect,
  // and is always drawn at the same size. This is so that placeholder icons are
  // visible (e.g. when replacing a large image that's scaled down to a small
  // area) and so that all placeholder images on the same page look consistent.
  canvas->drawImageRect(
      icon_image->PaintImageForCurrentFrame(),
      IntRect(IntPoint::Zero(), icon_image->Size()),
      FloatRect(x, y, scale_factor * kIconWidth, scale_factor * kIconHeight),
      &flags, cc::PaintCanvas::kFast_SrcRectConstraint);
}

void DrawCenteredIcon(cc::PaintCanvas* canvas,
                      const PaintFlags& flags,
                      const FloatRect& dest_rect,
                      float scale_factor) {
  DrawIcon(
      canvas, flags,
      dest_rect.X() + (dest_rect.Width() - scale_factor * kIconWidth) / 2.0f,
      dest_rect.Y() + (dest_rect.Height() - scale_factor * kIconHeight) / 2.0f,
      scale_factor);
}

FontDescription CreatePlaceholderFontDescription(float scale_factor) {
  FontDescription description;
  description.FirstFamily().SetFamily("Roboto");

  scoped_refptr<SharedFontFamily> helvetica_neue = SharedFontFamily::Create();
  helvetica_neue->SetFamily("Helvetica Neue");
  scoped_refptr<SharedFontFamily> helvetica = SharedFontFamily::Create();
  helvetica->SetFamily("Helvetica");
  scoped_refptr<SharedFontFamily> arial = SharedFontFamily::Create();
  arial->SetFamily("Arial");

  helvetica->AppendFamily(std::move(arial));
  helvetica_neue->AppendFamily(std::move(helvetica));
  description.FirstFamily().AppendFamily(std::move(helvetica_neue));

  description.SetGenericFamily(FontDescription::kSansSerifFamily);
  description.SetComputedSize(scale_factor * kFontSize);
  description.SetWeight(FontSelectionValue(500));

  return description;
}

// Return a byte quantity as a string in a localized human-readable format,
// suitable for being shown on a placeholder image to indicate the full original
// size of the resource.
//
// Ex: FormatOriginalResourceSizeBytes(100) => "1 KB"
// Ex: FormatOriginalResourceSizeBytes(102401) => "100 KB"
// Ex: FormatOriginalResourceSizeBytes(1740800) => "1.7 MB"
//
// See the placeholder image number format specifications for more info:
// https://docs.google.com/document/d/1BHeA1azbgCdZgCnr16VN2g7A9MHPQ_dwKn5szh8evMQ/edit#heading=h.d135l9z7tn0a
String FormatOriginalResourceSizeBytes(int64_t bytes) {
  DCHECK_LT(0, bytes);

  static constexpr int kUnitsResourceIds[] = {
      IDS_UNITS_KIBIBYTES, IDS_UNITS_MEBIBYTES, IDS_UNITS_GIBIBYTES,
      IDS_UNITS_TEBIBYTES, IDS_UNITS_PEBIBYTES};

  // Start with KB. The formatted text will be at least "1 KB", with any smaller
  // amounts being rounded up to "1 KB".
  const int* units = kUnitsResourceIds;
  int64_t denomenator = 1024;

  // Find the smallest unit that can represent |bytes| in 3 digits or less.
  // Round up to the next higher unit if possible when it would take 4 digits to
  // display the amount, e.g. 1000 KB will be rounded up to 1 MB.
  for (; units < kUnitsResourceIds + (base::size(kUnitsResourceIds) - 1) &&
         bytes >= denomenator * 1000;
       ++units, denomenator *= 1024) {
  }

  String numeric_string;
  if (bytes < denomenator) {
    // Round up to 1.
    numeric_string = String::Number(1);
  } else if (units != kUnitsResourceIds && bytes < denomenator * 10) {
    // For amounts between 1 and 10 units and larger than 1 MB, allow up to one
    // fractional digit.
    numeric_string = String::Number(
        static_cast<double>(bytes) / static_cast<double>(denomenator), 2);
  } else {
    numeric_string = String::Number(bytes / denomenator);
  }

  Locale& locale = Locale::DefaultLocale();
  // Locale::QueryString() will return an empty string if the embedder hasn't
  // defined the string resources for the units, which will cause the
  // PlaceholderImage to not show any text.
  return locale.QueryString(*units,
                            locale.ConvertToLocalizedNumber(numeric_string));
}

}  // namespace

// A simple RefCounted wrapper around a Font, so that multiple PlaceholderImages
// can share the same Font.
class PlaceholderImage::SharedFont : public RefCounted<SharedFont> {
 public:
  static scoped_refptr<SharedFont> GetOrCreateInstance(float scale_factor) {
    if (g_instance_) {
      scoped_refptr<SharedFont> shared_font(g_instance_);
      shared_font->MaybeUpdateForScaleFactor(scale_factor);
      return shared_font;
    }

    scoped_refptr<SharedFont> shared_font =
        base::MakeRefCounted<SharedFont>(scale_factor);
    g_instance_ = shared_font.get();
    return shared_font;
  }

  // This constructor is public so that base::MakeRefCounted() can call it.
  explicit SharedFont(float scale_factor)
      : font_(CreatePlaceholderFontDescription(scale_factor)),
        scale_factor_(scale_factor) {
    font_.Update(nullptr);
  }

  ~SharedFont() {
    DCHECK_EQ(this, g_instance_);
    g_instance_ = nullptr;
  }

  void MaybeUpdateForScaleFactor(float scale_factor) {
    if (scale_factor_ == scale_factor)
      return;

    scale_factor_ = scale_factor;
    font_ = Font(CreatePlaceholderFontDescription(scale_factor_));
    font_.Update(nullptr);
  }

  const Font& font() const { return font_; }

 private:
  static SharedFont* g_instance_;

  Font font_;
  float scale_factor_;
};

// static
PlaceholderImage::SharedFont* PlaceholderImage::SharedFont::g_instance_ =
    nullptr;

PlaceholderImage::PlaceholderImage(ImageObserver* observer,
                                   const IntSize& size,
                                   int64_t original_resource_size,
                                   bool is_lazy_image)
    : Image(observer),
      size_(size),
      text_(original_resource_size <= 0
                ? String()
                : FormatOriginalResourceSizeBytes(original_resource_size)),
      is_lazy_image_(is_lazy_image),
      paint_record_content_id_(-1) {}

PlaceholderImage::~PlaceholderImage() = default;

IntSize PlaceholderImage::Size() const {
  return size_;
}

bool PlaceholderImage::IsPlaceholderImage() const {
  return true;
}

bool PlaceholderImage::CurrentFrameHasSingleSecurityOrigin() const {
  return true;
}

bool PlaceholderImage::CurrentFrameKnownToBeOpaque() {
  // Placeholder images are translucent.
  return false;
}

PaintImage PlaceholderImage::PaintImageForCurrentFrame() {
  auto builder = CreatePaintImageBuilder().set_completion_state(
      PaintImage::CompletionState::DONE);

  const IntRect dest_rect(0, 0, size_.Width(), size_.Height());
  if (paint_record_for_current_frame_) {
    return builder
        .set_paint_record(paint_record_for_current_frame_, dest_rect,
                          paint_record_content_id_)
        .TakePaintImage();
  }

  PaintRecorder paint_recorder;
  Draw(paint_recorder.beginRecording(FloatRect(dest_rect)), PaintFlags(),
       FloatRect(dest_rect), FloatRect(dest_rect),
       kDoNotRespectImageOrientation, kClampImageToSourceRect, kSyncDecode);

  paint_record_for_current_frame_ = paint_recorder.finishRecordingAsPicture();
  paint_record_content_id_ = PaintImage::GetNextContentId();
  return builder
      .set_paint_record(paint_record_for_current_frame_, dest_rect,
                        paint_record_content_id_)
      .TakePaintImage();
}

void PlaceholderImage::SetIconAndTextScaleFactor(
    float icon_and_text_scale_factor) {
  if (icon_and_text_scale_factor_ == icon_and_text_scale_factor)
    return;
  icon_and_text_scale_factor_ = icon_and_text_scale_factor;
  cached_text_width_.reset();
  paint_record_for_current_frame_.reset();
}

void PlaceholderImage::Draw(cc::PaintCanvas* canvas,
                            const PaintFlags& base_flags,
                            const FloatRect& dest_rect,
                            const FloatRect& src_rect,
                            RespectImageOrientationEnum respect_orientation,
                            ImageClampingMode image_clamping_mode,
                            ImageDecodingMode decode_mode) {
  if (!src_rect.Intersects(FloatRect(0.0f, 0.0f,
                                     static_cast<float>(size_.Width()),
                                     static_cast<float>(size_.Height())))) {
    return;
  }
  if (is_lazy_image_) {
    // Keep the image without any color and text decorations.
    return;
  }

  PaintFlags flags(base_flags);
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SkColorSetARGB(0x80, 0xD9, 0xD9, 0xD9));
  canvas->drawRect(dest_rect, flags);

  if (dest_rect.Width() <
          icon_and_text_scale_factor_ * (kIconWidth + 2 * kFeaturePaddingX) ||
      dest_rect.Height() <
          icon_and_text_scale_factor_ * (kIconHeight + 2 * kIconPaddingY)) {
    return;
  }

  if (text_.IsEmpty()) {
    DrawCenteredIcon(canvas, base_flags, dest_rect,
                     icon_and_text_scale_factor_);
    return;
  }

  if (!shared_font_)
    shared_font_ = SharedFont::GetOrCreateInstance(icon_and_text_scale_factor_);
  else
    shared_font_->MaybeUpdateForScaleFactor(icon_and_text_scale_factor_);

  if (!cached_text_width_.has_value())
    cached_text_width_ = shared_font_->font().Width(TextRun(text_));

  const float icon_and_text_width =
      cached_text_width_.value() +
      icon_and_text_scale_factor_ *
          (kIconWidth + 2 * kFeaturePaddingX + kPaddingBetweenIconAndText);

  if (dest_rect.Width() < icon_and_text_width) {
    DrawCenteredIcon(canvas, base_flags, dest_rect,
                     icon_and_text_scale_factor_);
    return;
  }

  const float feature_x =
      dest_rect.X() + (dest_rect.Width() - icon_and_text_width) / 2.0f;
  const float feature_y =
      dest_rect.Y() +
      (dest_rect.Height() -
       icon_and_text_scale_factor_ * (kIconHeight + 2 * kIconPaddingY)) /
          2.0f;

  float icon_x, text_x;
  if (Locale::DefaultLocale().IsRTL()) {
    icon_x = feature_x + cached_text_width_.value() +
             icon_and_text_scale_factor_ *
                 (kFeaturePaddingX + kPaddingBetweenIconAndText);
    text_x = feature_x + icon_and_text_scale_factor_ * kFeaturePaddingX;
  } else {
    icon_x = feature_x + icon_and_text_scale_factor_ * kFeaturePaddingX;
    text_x = feature_x +
             icon_and_text_scale_factor_ *
                 (kFeaturePaddingX + kIconWidth + kPaddingBetweenIconAndText);
  }

  DrawIcon(canvas, base_flags, icon_x,
           feature_y + icon_and_text_scale_factor_ * kIconPaddingY,
           icon_and_text_scale_factor_);

  flags.setColor(SkColorSetARGB(0xAB, 0, 0, 0));
  shared_font_->font().DrawBidiText(
      canvas, TextRunPaintInfo(TextRun(text_)),
      FloatPoint(text_x, feature_y + icon_and_text_scale_factor_ *
                                         (kTextPaddingY + kFontSize)),
      Font::kUseFallbackIfFontNotReady, 1.0f, flags);
}

void PlaceholderImage::DrawPattern(GraphicsContext& context,
                                   const FloatRect& src_rect,
                                   const FloatSize& scale,
                                   const FloatPoint& phase,
                                   SkBlendMode mode,
                                   const FloatRect& dest_rect,
                                   const FloatSize& repeat_spacing) {
  DCHECK(context.Canvas());

  PaintFlags flags = context.FillFlags();
  flags.setBlendMode(mode);

  // Ignore the pattern specifications and just draw a single placeholder image
  // over the whole |dest_rect|. This is done in order to prevent repeated icons
  // from cluttering tiled background images.
  Draw(context.Canvas(), flags, dest_rect, src_rect,
       kDoNotRespectImageOrientation, kClampImageToSourceRect,
       kUnspecifiedDecode);
}

void PlaceholderImage::DestroyDecodedData() {
  paint_record_for_current_frame_.reset();
  shared_font_ = scoped_refptr<SharedFont>();
}

Image::SizeAvailability PlaceholderImage::SetData(scoped_refptr<SharedBuffer>,
                                                  bool) {
  return Image::kSizeAvailable;
}

const Font* PlaceholderImage::GetFontForTesting() const {
  return shared_font_ ? &shared_font_->font() : nullptr;
}

}  // namespace blink
