// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_crossfade_image.h"

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/platform/graphics/crossfade_generated_image.h"

namespace blink {

StyleCrossfadeImage::StyleCrossfadeImage(cssvalue::CSSCrossfadeValue& value,
                                         HeapVector<Member<StyleImage>> images)
    : original_value_(value), images_(std::move(images)) {
  is_crossfade_ = true;
}

StyleCrossfadeImage::~StyleCrossfadeImage() = default;

bool StyleCrossfadeImage::IsEqual(const StyleImage& other) const {
  if (!other.IsCrossfadeImage()) {
    return false;
  }
  return original_value_ == To<StyleCrossfadeImage>(other).original_value_;
}

CSSValue* StyleCrossfadeImage::CssValue() const {
  return original_value_.Get();
}

CSSValue* StyleCrossfadeImage::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  // If either of the images are null (meaning that they are 'none'),
  // then use the original value. This is only possible in the older
  // -webkit-cross-fade version; the newer does not allow it.
  HeapVector<std::pair<Member<CSSValue>, Member<CSSPrimitiveValue>>>
      image_and_percentages;
  for (unsigned i = 0; i < images_.size(); ++i) {
    CSSValue* value =
        images_[i] ? images_[i]->ComputedCSSValue(style, allow_visited_style,
                                                  value_phase)
                   : original_value_->GetImagesAndPercentages()[i].first.Get();
    CSSPrimitiveValue* percentage =
        original_value_->GetImagesAndPercentages()[i].second;
    if (percentage && !percentage->IsNumericLiteralValue()) {
      // https://drafts.csswg.org/css-cascade-5/#computed-value
      double val = ClampTo<double>(percentage->GetDoubleValue(), 0.0, 100.0);
      percentage = CSSNumericLiteralValue::Create(
          val, CSSPrimitiveValue::UnitType::kPercentage);
    }
    image_and_percentages.emplace_back(value, percentage);
  }
  return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
      original_value_->IsPrefixedVariant(), std::move(image_and_percentages));
}

bool StyleCrossfadeImage::CanRender() const {
  return std::all_of(images_.begin(), images_.end(), [](StyleImage* image) {
    return !image || image->CanRender();
  });
}

bool StyleCrossfadeImage::IsLoading() const {
  return std::any_of(images_.begin(), images_.end(), [](StyleImage* image) {
    return image && image->IsLoading();
  });
}

bool StyleCrossfadeImage::IsLoaded() const {
  return std::all_of(images_.begin(), images_.end(), [](StyleImage* image) {
    return !image || image->IsLoaded();
  });
}

bool StyleCrossfadeImage::ErrorOccurred() const {
  return std::any_of(images_.begin(), images_.end(), [](StyleImage* image) {
    return image && image->ErrorOccurred();
  });
}

bool StyleCrossfadeImage::IsAccessAllowed(String& failing_url) const {
  return std::all_of(images_.begin(), images_.end(), [&](StyleImage* image) {
    return !image || image->IsAccessAllowed(failing_url);
  });
}

bool StyleCrossfadeImage::AnyImageIsNone() const {
  return std::any_of(images_.begin(), images_.end(),
                     [](StyleImage* image) { return !image; });
}

// Only <image> values participate in the sizing (§2.6.1.2).
// In this aspect, the standard seems to indicate everything
// that is not a <color> is an <image>.
static bool ParticipatesInSizing(const CSSValue* image) {
  return !image->IsConstantGradientValue();
}

static bool ParticipatesInSizing(const StyleImage& image) {
  if (IsA<StyleGeneratedImage>(image)) {
    return ParticipatesInSizing(To<StyleGeneratedImage>(image).CssValue());
  }
  return true;
}

// https://drafts.csswg.org/css-images-4/#cross-fade-sizing
IntrinsicSizingInfo StyleCrossfadeImage::GetNaturalSizingInfo(
    float multiplier,
    RespectImageOrientationEnum respect_orientation) const {
  if (AnyImageIsNone()) {
    return IntrinsicSizingInfo::None();
  }

  // TODO(fs): Consider `respect_orientation`?
  Vector<IntrinsicSizingInfo> sizing_info;
  for (StyleImage* image : images_) {
    if (ParticipatesInSizing(*image)) {
      sizing_info.push_back(
          image->GetNaturalSizingInfo(multiplier, kRespectImageOrientation));
    }
  }

  // Degenerate cases.
  if (sizing_info.empty()) {
    return IntrinsicSizingInfo::None();
  } else if (sizing_info.size() == 1) {
    return sizing_info[0];
  }

  // (See `StyleCrossfadeImage::ImageSize()`)
  const bool all_equal = std::ranges::all_of(
      base::span(sizing_info).subspan(1u),
      [first_sizing_info{sizing_info[0]}](
          const IntrinsicSizingInfo& sizing_info) {
        return sizing_info.size == first_sizing_info.size &&
               sizing_info.aspect_ratio == first_sizing_info.aspect_ratio &&
               sizing_info.has_width == first_sizing_info.has_width &&
               sizing_info.has_height == first_sizing_info.has_height;
      });
  if (all_equal) {
    return sizing_info[0];
  }

  const std::vector<float> weights = ComputeWeights(/*for_sizing=*/true);
  IntrinsicSizingInfo result_sizing_info;
  result_sizing_info.size = gfx::SizeF(0.0f, 0.0f);
  result_sizing_info.has_width = false;
  result_sizing_info.has_height = false;
  DCHECK_EQ(weights.size(), sizing_info.size());
  for (unsigned i = 0; i < sizing_info.size(); ++i) {
    result_sizing_info.size +=
        gfx::SizeF(sizing_info[i].size.width() * weights[i],
                   sizing_info[i].size.height() * weights[i]);
    result_sizing_info.has_width |= sizing_info[i].has_width;
    result_sizing_info.has_height |= sizing_info[i].has_height;
  }
  if (result_sizing_info.has_width && result_sizing_info.has_height) {
    result_sizing_info.aspect_ratio = result_sizing_info.size;
  }
  return result_sizing_info;
}

gfx::SizeF StyleCrossfadeImage::ImageSize(float multiplier,
                                          const gfx::SizeF& default_object_size,
                                          RespectImageOrientationEnum) const {
  if (AnyImageIsNone()) {
    return gfx::SizeF();
  }

  // TODO(fs): Consider |respect_orientation|?
  Vector<gfx::SizeF> image_sizes;
  for (StyleImage* image : images_) {
    if (ParticipatesInSizing(*image)) {
      image_sizes.push_back(image->ImageSize(multiplier, default_object_size,
                                             kRespectImageOrientation));
    }
  }

  // Degenerate cases.
  if (image_sizes.empty()) {
    // If we have only solid colors, there is no natural size, but we still
    // need to have an actual size of at least 1x1 to get anything on screen.
    return images_.empty() ? gfx::SizeF() : gfx::SizeF(1.0f, 1.0f);
  } else if (image_sizes.size() == 1) {
    return image_sizes[0];
  }

  // Rounding issues can cause transitions between images of equal size to
  // return a different fixed size; avoid performing the interpolation if the
  // images are the same size.
  const bool all_equal = std::ranges::all_of(
      base::span(image_sizes).subspan(1u),
      [first_image_size{image_sizes[0]}](const gfx::SizeF& image_size) {
        return image_size == first_image_size;
      });
  if (all_equal) {
    return image_sizes[0];
  }

  const std::vector<float> weights = ComputeWeights(/*for_sizing=*/true);
  gfx::SizeF size(0.0f, 0.0f);
  DCHECK_EQ(weights.size(), image_sizes.size());
  for (unsigned i = 0; i < image_sizes.size(); ++i) {
    size += gfx::SizeF(image_sizes[i].width() * weights[i],
                       image_sizes[i].height() * weights[i]);
  }
  return size;
}

bool StyleCrossfadeImage::HasIntrinsicSize() const {
  return std::any_of(images_.begin(), images_.end(), [](StyleImage* image) {
    return image && image->HasIntrinsicSize();
  });
}

void StyleCrossfadeImage::AddClient(ImageResourceObserver* observer) {
  const bool had_clients = original_value_->HasClients();
  original_value_->AddClient(observer);
  if (had_clients) {
    return;
  }
  ImageResourceObserver* proxy_observer = original_value_->GetObserverProxy();
  for (StyleImage* image : images_) {
    if (image) {
      image->AddClient(proxy_observer);
    }
  }
}

void StyleCrossfadeImage::RemoveClient(ImageResourceObserver* observer) {
  original_value_->RemoveClient(observer);
  if (original_value_->HasClients()) {
    return;
  }
  ImageResourceObserver* proxy_observer = original_value_->GetObserverProxy();
  for (StyleImage* image : images_) {
    if (image) {
      image->RemoveClient(proxy_observer);
    }
  }
}

scoped_refptr<Image> StyleCrossfadeImage::GetImage(
    const ImageResourceObserver& observer,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) const {
  if (target_size.IsEmpty()) {
    return nullptr;
  }
  if (AnyImageIsNone()) {
    return Image::NullImage();
  }
  const gfx::SizeF resolved_size =
      ImageSize(style.EffectiveZoom(), target_size, kRespectImageOrientation);
  const ImageResourceObserver* proxy_observer =
      original_value_->GetObserverProxy();

  const std::vector<float> weights = ComputeWeights(/*for_sizing=*/false);
  Vector<CrossfadeGeneratedImage::WeightedImage> images;
  DCHECK_EQ(images_.size(), weights.size());
  for (unsigned i = 0; i < images_.size(); ++i) {
    scoped_refptr<Image> image =
        images_[i]->GetImage(*proxy_observer, document, style, target_size);
    images.push_back(
        CrossfadeGeneratedImage::WeightedImage{std::move(image), weights[i]});
  }
  return CrossfadeGeneratedImage::Create(std::move(images), resolved_size);
}

WrappedImagePtr StyleCrossfadeImage::Data() const {
  return original_value_.Get();
}

bool StyleCrossfadeImage::KnownToBeOpaque(const Document& document,
                                          const ComputedStyle& style) const {
  return std::all_of(images_.begin(), images_.end(), [&](StyleImage* image) {
    return image && image->KnownToBeOpaque(document, style);
  });
}

// Calculates the actual value of the percentage for each image,
// and converts to 0..1 weights. See
// https://drafts.csswg.org/css-images-4/#cross-fade-function:
//
// “If any percentages are omitted, all the specified percentages are summed
// together and subtracted from 100%, the result is floored at 0%, then divided
// equally between all images with omitted percentages at computed-value time.”
std::vector<float> StyleCrossfadeImage::ComputeWeights(bool for_sizing) const {
  std::vector<float> result;
  float sum = 0.0f;
  int num_missing = 0;

  for (const auto& [image, percentage] :
       original_value_->GetImagesAndPercentages()) {
    if (for_sizing && !ParticipatesInSizing(image)) {
      continue;
    }
    if (percentage == nullptr) {
      result.push_back(0.0 / 0.0);  // NaN.
      ++num_missing;
    } else if (percentage->IsPercentage()) {
      result.push_back(percentage->GetFloatValue() / 100.0);
      sum += result.back();
    } else {
      result.push_back(percentage->GetFloatValue());
      sum += result.back();
    }
  }
  if (num_missing > 0) {
    float equal_share = std::max(1.0f - sum, 0.0f) / num_missing;
    for (float& weight : result) {
      if (isnan(weight)) {
        weight = equal_share;
      }
    }
    sum = std::max(sum, 1.0f);
  }
  if (for_sizing && sum != 1.0f && sum > 0.0f) {
    // §2.6.1.5. For each item in images, divide item’s percentage
    // by percentage sum, and set item’s percentage to the result.
    for (float& percentage : result) {
      percentage /= sum;
    }
  } else if (!for_sizing && sum > 1.0f) {
    // §2.6.2.5. […] Otherwise, if percentage sum is greater than 100%,
    // then for each item in images, divide item’s percentage by
    // percentage sum, and set item’s percentage to the result.
    //
    // NOTE: If the sum is _less_ than 100%, the end result is
    // not normalized (see the rest of 2.6.2.5).
    for (float& percentage : result) {
      percentage /= sum;
    }
  }
  return result;
}

void StyleCrossfadeImage::Trace(Visitor* visitor) const {
  visitor->Trace(original_value_);
  visitor->Trace(images_);
  StyleImage::Trace(visitor);
}

}  // namespace blink
