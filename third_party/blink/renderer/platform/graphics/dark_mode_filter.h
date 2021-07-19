// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_

#include <memory>

#include "base/stl_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"

class SkColorFilter;
class SkPixmap;

namespace blink {

class GraphicsContext;
class DarkModeColorClassifier;
class DarkModeImageClassifier;
class DarkModeColorFilter;
class ScopedDarkModeElementRoleOverride;
class DarkModeInvertedColorCache;

class PLATFORM_EXPORT DarkModeFilter {
 public:
  // Dark mode is disabled by default. Enable it by calling UpdateSettings()
  // with a mode other than DarkMode::kOff.
  explicit DarkModeFilter(const DarkModeSettings& settings);
  ~DarkModeFilter();

  // TODO(gilmanmh): Add a role for shadows. In general, we don't want to
  // invert shadows, but we may need to do some other kind of processing for
  // them.
  enum class ElementRole { kText, kListSymbol, kBackground, kSVG };

  SkColor InvertColorIfNeeded(SkColor color, ElementRole element_role);
  absl::optional<cc::PaintFlags> ApplyToFlagsIfNeeded(
      const cc::PaintFlags& flags,
      ElementRole element_role);

  size_t GetInvertedColorCacheSizeForTesting();

  // Decides whether to apply dark mode or not based on |src| and |dst|.
  // DarkModeResult::kDoNotApplyFilter - Dark mode filter should not be applied.
  // DarkModeResult::kApplyFilter - Dark mode filter should be applied and to
  // get the color filter GetImageFilter() should be called.
  // DarkModeResult::kNotClassified - Dark mode filter should be applied and to
  // get the color filter ApplyToImage() should be called. This API is
  // thread-safe.
  DarkModeResult AnalyzeShouldApplyToImage(const SkIRect& src,
                                           const SkIRect& dst) const;

  // Returns dark mode color filter based on the classification done on
  // |pixmap|. The image cannot be classified if pixmap is empty or |src| is
  // empty or |src| is larger than pixmap bounds. Before calling this function
  // AnalyzeShouldApplyToImage() must be called for early out or deciding
  // appropriate function call. This function should be called only if image
  // policy is set to DarkModeImagePolicy::kFilterSmart. This API is
  // thread-safe.
  sk_sp<SkColorFilter> ApplyToImage(const SkPixmap& pixmap,
                                    const SkIRect& src) const;

  // Returns dark mode color filter for images. Before calling this function
  // AnalyzeShouldApplyToImage() must be called for early out or deciding
  // appropriate function call. This function should be called only if image
  // policy is set to DarkModeImagePolicy::kFilterAll. This API is thread-safe.
  sk_sp<SkColorFilter> GetImageFilter() const;

 private:
  friend class ScopedDarkModeElementRoleOverride;

  struct ImmutableData {
    explicit ImmutableData(const DarkModeSettings& settings);

    DarkModeSettings settings;
    std::unique_ptr<DarkModeColorClassifier> text_classifier;
    std::unique_ptr<DarkModeColorClassifier> background_classifier;
    std::unique_ptr<DarkModeImageClassifier> image_classifier;
    std::unique_ptr<DarkModeColorFilter> color_filter;
    sk_sp<SkColorFilter> image_filter;
  };

  bool ShouldApplyToColor(SkColor color, ElementRole role);

  // This is read-only data and is thread-safe.
  const ImmutableData immutable_;

  // Following two members used for color classifications are not thread-safe.
  // TODO(prashant.n): Remove element override concept.
  absl::optional<ElementRole> role_override_;
  // TODO(prashant.n): Move cache out of dark mode filter.
  std::unique_ptr<DarkModeInvertedColorCache> inverted_color_cache_;
};

// Temporarily override the element role for the scope of this object's
// lifetime - for example when drawing symbols that play the role of text.
class PLATFORM_EXPORT ScopedDarkModeElementRoleOverride {
 public:
  ScopedDarkModeElementRoleOverride(GraphicsContext* graphics_context,
                                    DarkModeFilter::ElementRole role);
  ~ScopedDarkModeElementRoleOverride();

 private:
  GraphicsContext* graphics_context_;
  absl::optional<DarkModeFilter::ElementRole> previous_role_override_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_FILTER_H_
