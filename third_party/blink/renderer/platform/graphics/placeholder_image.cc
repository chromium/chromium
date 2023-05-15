// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/placeholder_image.h"

#include <utility>

#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

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
              const cc::PaintFlags& flags,
              float x,
              float y,
              const SkSamplingOptions& sampling,
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
      SkRect::MakeWH(icon_image->width(), icon_image->height()),
      SkRect::MakeXYWH(x, y, scale_factor * kIconWidth,
                       scale_factor * kIconHeight),
      sampling, &flags, SkCanvas::kFast_SrcRectConstraint);
}

void DrawCenteredIcon(cc::PaintCanvas* canvas,
                      const cc::PaintFlags& flags,
                      const gfx::RectF& dest_rect,
                      const SkSamplingOptions& sampling,
                      float scale_factor) {
  DrawIcon(
      canvas, flags,
      dest_rect.x() + (dest_rect.width() - scale_factor * kIconWidth) / 2.0f,
      dest_rect.y() + (dest_rect.height() - scale_factor * kIconHeight) / 2.0f,
      sampling, scale_factor);
}

FontDescription CreatePlaceholderFontDescription(float scale_factor) {
  FontDescription description;
  description.FirstFamily().SetFamily(font_family_names::kRoboto,
                                      FontFamily::Type::kFamilyName);

  scoped_refptr<SharedFontFamily> helvetica_neue = SharedFontFamily::Create();
  helvetica_neue->SetFamily(font_family_names::kHelveticaNeue,
                            FontFamily::Type::kFamilyName);
  scoped_refptr<SharedFontFamily> helvetica = SharedFontFamily::Create();
  helvetica->SetFamily(font_family_names::kHelvetica,
                       FontFamily::Type::kFamilyName);
  scoped_refptr<SharedFontFamily> arial = SharedFontFamily::Create();
  arial->SetFamily(font_family_names::kArial, FontFamily::Type::kFamilyName);

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
  for (; units < kUnitsResourceIds + (std::size(kUnitsResourceIds) - 1) &&
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
                                   const gfx::Size& size,
                                   int64_t original_resource_size)
    : Image(observer),
      size_(size),
      text_(original_resource_size <= 0
                ? String()
                : FormatOriginalResourceSizeBytes(original_resource_size)),
      paint_record_content_id_(-1) {}

PlaceholderImage::~PlaceholderImage() = default;

gfx::Size PlaceholderImage::SizeWithConfig(SizeConfig) const {
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

  const gfx::Rect dest_rect(size_);
  if (paint_record_for_current_frame_) {
    return builder
        .set_paint_record(*paint_record_for_current_frame_, dest_rect,
                          paint_record_content_id_)
        .TakePaintImage();
  }

  PaintRecorder paint_recorder;
  Draw(paint_recorder.beginRecording(), cc::PaintFlags(), gfx::RectF(dest_rect),
       gfx::RectF(dest_rect), ImageDrawOptions());

  paint_record_for_current_frame_ = paint_recorder.finishRecordingAsPicture();
  paint_record_content_id_ = PaintImage::GetNextContentId();
  return builder
      .set_paint_record(*paint_record_for_current_frame_, dest_rect,
                        paint_record_content_id_)
      .TakePaintImage();
}

void PlaceholderImage::SetIconAndTextScaleFactor(
    float icon_and_text_scale_factor) {
  if (icon_and_text_scale_factor_ == icon_and_text_scale_factor)
    return;
  icon_and_text_scale_factor_ = icon_and_text_scale_factor;
  cached_text_width_.reset();
  paint_record_for_current_frame_ = absl::nullopt;
}

void PlaceholderImage::Draw(cc::PaintCanvas* canvas,
                            const cc::PaintFlags& base_flags,
                            const gfx::RectF& dest_rect,
                            const gfx::RectF& src_rect,
                            const ImageDrawOptions& draw_options) {
  if (!src_rect.Intersects(gfx::RectF(0.0f, 0.0f,
                                      static_cast<float>(size_.width()),
                                      static_cast<float>(size_.height())))) {
    return;
  }

  cc::PaintFlags flags(base_flags);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColorSetARGB(0x80, 0xD9, 0xD9, 0xD9));
  canvas->drawRect(gfx::RectFToSkRect(dest_rect), flags);

  if (dest_rect.width() <
          icon_and_text_scale_factor_ * (kIconWidth + 2 * kFeaturePaddingX) ||
      dest_rect.height() <
          icon_and_text_scale_factor_ * (kIconHeight + 2 * kIconPaddingY)) {
    return;
  }

  if (text_.empty()) {
    DrawCenteredIcon(canvas, base_flags, dest_rect,
                     draw_options.sampling_options,
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

  if (dest_rect.width() < icon_and_text_width) {
    DrawCenteredIcon(canvas, base_flags, dest_rect,
                     draw_options.sampling_options,
                     icon_and_text_scale_factor_);
    return;
  }

  const float feature_x =
      dest_rect.x() + (dest_rect.width() - icon_and_text_width) / 2.0f;
  const float feature_y =
      dest_rect.y() +
      (dest_rect.height() -
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
           draw_options.sampling_options, icon_and_text_scale_factor_);

  flags.setColor(SkColorSetARGB(0xAB, 0, 0, 0));
  shared_font_->font().DrawBidiText(
      canvas, TextRunPaintInfo(TextRun(text_)),
      gfx::PointF(text_x, feature_y + icon_and_text_scale_factor_ *
                                          (kTextPaddingY + kFontSize)),
      Font::kUseFallbackIfFontNotReady, flags);
}

void PlaceholderImage::DrawPattern(GraphicsContext& context,
                                   const cc::PaintFlags& base_flags,
                                   const gfx::RectF& dest_rect,
                                   const ImageTilingInfo& tiling_info,
                                   const ImageDrawOptions& draw_options) {
  DCHECK(context.Canvas());
  // Ignore the pattern specifications and just draw a single placeholder image
  // over the whole |dest_rect|. This is done in order to prevent repeated icons
  // from cluttering tiled background images.
  Draw(context.Canvas(), base_flags, dest_rect, tiling_info.image_rect,
       draw_options);
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
