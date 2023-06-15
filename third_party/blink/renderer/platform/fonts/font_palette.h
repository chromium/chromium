// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_

#include <memory>
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {

/* FontPalette stores CSS font-palette information in a
 * FontDescription. It's used for representing the computed style
 * information which can contain either light, dark or custom palette
 * information according to the font-palette property. */
class PLATFORM_EXPORT FontPalette : public RefCounted<FontPalette> {
 public:
  enum KeywordPaletteName {
    kNormalPalette = 0,
    kLightPalette = 1,
    kDarkPalette = 2,
    kCustomPalette = 3,
    kInterpolablePalette = 4,
  };

  // Data layout should match SkFontarguments::PaletteOverride::ColorOverride.
  struct FontPaletteOverride {
    int index;
    Color color;

    bool operator==(const FontPaletteOverride& other) const {
      return index == other.index && color == other.color;
    }
    DISALLOW_NEW();
  };

  enum BasePaletteValueType {
    kNoBasePalette,
    kLightBasePalette,
    kDarkBasePalette,
    kIndexBasePalette,
  };

  struct BasePaletteValue {
    BasePaletteValueType type;
    int index;

    bool hasValue() { return type != kNoBasePalette; }
    bool operator==(const BasePaletteValue& other) const {
      return type == other.type && index == other.index;
    }
    DISALLOW_NEW();
  };

  static scoped_refptr<FontPalette> Create() {
    return base::AdoptRef(new FontPalette());
  }

  static scoped_refptr<FontPalette> Create(KeywordPaletteName palette_name) {
    // Use AtomicString constructor for custom palette instantiation.
    DCHECK(palette_name != kCustomPalette);
    return base::AdoptRef(new FontPalette(palette_name));
  }

  static scoped_refptr<FontPalette> Create(AtomicString palette_values_name) {
    return base::AdoptRef(new FontPalette(std::move(palette_values_name)));
  }

  // We introduce a mix-palette function to present interpolated font-palette
  // values at each frame, i.e. font-palette property’s value at time 0.5
  // between the palettes “--p1” and “--p2” will be presented as
  // mix-palettes(--p1, –p2, 0.5).
  static scoped_refptr<FontPalette> Mix(
      scoped_refptr<FontPalette> start,
      scoped_refptr<FontPalette> end,
      double percentage,
      double alpha_multiplier,
      Color::ColorSpace color_interpolation_space,
      absl::optional<Color::HueInterpolationMethod> hue_interpolation_method) {
    return base::AdoptRef(
        new FontPalette(start, end, percentage, alpha_multiplier,
                        color_interpolation_space, hue_interpolation_method));
  }

  void SetBasePalette(BasePaletteValue base_palette) {
    base_palette_ = base_palette;
  }

  void SetColorOverrides(Vector<FontPaletteOverride>&& overrides) {
    palette_overrides_ = overrides;
  }

  bool IsNormalPalette() const { return palette_keyword_ == kNormalPalette; }
  bool IsCustomPalette() const { return palette_keyword_ == kCustomPalette; }
  bool IsInterpolablePalette() const {
    return palette_keyword_ == kInterpolablePalette;
  }
  KeywordPaletteName GetPaletteNameKind() const { return palette_keyword_; }

  /* Returns the identifier of the @font-palette-values rule that should be
   * retrieved to complete the palette selection or palette override information
   * for this FontPalette object. */
  const AtomicString& GetPaletteValuesName() const {
    DCHECK(palette_keyword_ == kCustomPalette);
    return palette_values_name_;
  }

  const Vector<FontPaletteOverride>* GetColorOverrides() const {
    return &palette_overrides_;
  }

  BasePaletteValue GetBasePalette() const { return base_palette_; }

  void SetMatchFamilyName(AtomicString family_name) {
    match_font_family_ = family_name;
  }

  AtomicString GetMatchFamilyName() { return match_font_family_; }

  scoped_refptr<FontPalette> GetStart() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return start_;
  }

  scoped_refptr<FontPalette> GetEnd() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return end_;
  }

  double GetPercentage() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return percentage_;
  }

  double GetAlphaMultiplier() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK((IsInterpolablePalette()));
    return alpha_multiplier_;
  }

  Color::ColorSpace GetColorInterpolationSpace() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return color_interpolation_space_;
  }

  absl::optional<Color::HueInterpolationMethod> GetHueInterpolationMethod()
      const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return hue_interpolation_method_;
  }

  String ToString() const;

  bool operator==(const FontPalette& other) const;
  bool operator!=(const FontPalette& other) const { return !(*this == other); }

  unsigned GetHash() const;

 private:
  explicit FontPalette(KeywordPaletteName palette_name)
      : palette_keyword_(palette_name), base_palette_({kNoBasePalette, 0}) {}
  explicit FontPalette(AtomicString palette_values_name)
      : palette_keyword_(kCustomPalette),
        palette_values_name_(palette_values_name),
        base_palette_({kNoBasePalette, 0}) {}
  FontPalette(
      scoped_refptr<FontPalette> start,
      scoped_refptr<FontPalette> end,
      double percentage,
      double alpha_multiplier,
      Color::ColorSpace color_interpoaltion_space,
      absl::optional<Color::HueInterpolationMethod> hue_interpolation_method)
      : palette_keyword_(kInterpolablePalette),
        start_(start),
        end_(end),
        percentage_(percentage),
        alpha_multiplier_(alpha_multiplier),
        color_interpolation_space_(color_interpoaltion_space),
        hue_interpolation_method_(hue_interpolation_method) {}
  FontPalette()
      : palette_keyword_(kNormalPalette), base_palette_({kNoBasePalette, 0}) {}

  KeywordPaletteName palette_keyword_;
  AtomicString palette_values_name_;
  BasePaletteValue base_palette_;
  AtomicString match_font_family_;
  Vector<FontPaletteOverride> palette_overrides_;
  scoped_refptr<FontPalette> start_;
  scoped_refptr<FontPalette> end_;
  double percentage_;
  double alpha_multiplier_;
  Color::ColorSpace color_interpolation_space_;
  absl::optional<Color::HueInterpolationMethod> hue_interpolation_method_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_
