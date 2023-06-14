// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_

#include <memory>
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

  enum InterpolablePaletteOperationType {
    kNoInterpolation,
    kMixPalettes,
    kScalePalette,
    kAddPalettes
  };

  // We introduce a mix-palette function to present interpolated font-palette
  // values at each frame, i.e. font-palette property’s value at time 0.5
  // between the palettes “--p1” and “--p2” will be presented as
  // mix-palettes(--p1, –p2, 0.5). We also introduce palette-scale() and
  // palette-add() functions for additive animations’ implementation. Function
  // palette-add(--p1, --p2) adds colors under the same index from "--p1" and
  // "--p2" pelettes' color records lists as it is currently done for
  // interpolable color properties. Function palette-scale(--p) is scaling each
  // color from the "--p" palette's color records list by the given scaling
  // parameter.
  struct InterpolablePaletteOperation {
    InterpolablePaletteOperationType type;
    double param = 0;

    bool hasValue() const { return type != kNoInterpolation; }
    String ToString() const {
      switch (type) {
        case kMixPalettes:
          return "palette-mix";
        case kScalePalette:
          return "palette-scale";
        case kAddPalettes:
          return "palette-add";
        case kNoInterpolation:
          NOTREACHED();
      }
      NOTREACHED();
      return "";
    }
    bool operator==(const InterpolablePaletteOperation& other) const {
      return type == other.type && param == other.param;
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

  static scoped_refptr<FontPalette> Mix(scoped_refptr<FontPalette> start,
                                        scoped_refptr<FontPalette> end,
                                        double progress) {
    return base::AdoptRef(
        new FontPalette(start, end, {kMixPalettes, progress}));
  }

  static scoped_refptr<FontPalette> Add(scoped_refptr<FontPalette> start,
                                        scoped_refptr<FontPalette> end) {
    return base::AdoptRef(new FontPalette(start, end, {kAddPalettes, 0}));
  }

  static scoped_refptr<FontPalette> Scale(scoped_refptr<FontPalette> palette,
                                          double scale) {
    return base::AdoptRef(
        new FontPalette(palette, palette, {kScalePalette, scale}));
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
    DCHECK(operation_.hasValue());
    return start_;
  }

  scoped_refptr<FontPalette> GetEnd() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    DCHECK(operation_.hasValue());
    return end_;
  }

  InterpolablePaletteOperation GetOperation() const {
    DCHECK(RuntimeEnabledFeatures::FontPaletteAnimationEnabled());
    DCHECK(IsInterpolablePalette());
    return operation_;
  }

  String ToString() const;

  bool operator==(const FontPalette& other) const;
  bool operator!=(const FontPalette& other) const { return !(*this == other); }

  unsigned GetHash() const;

 private:
  explicit FontPalette(KeywordPaletteName palette_name)
      : palette_keyword_(palette_name),
        base_palette_({kNoBasePalette, 0}),
        operation_({kNoInterpolation, 0}) {}
  explicit FontPalette(AtomicString palette_values_name)
      : palette_keyword_(kCustomPalette),
        palette_values_name_(palette_values_name),
        base_palette_({kNoBasePalette, 0}),
        operation_({kNoInterpolation, 0}) {}
  FontPalette(scoped_refptr<FontPalette> start,
              scoped_refptr<FontPalette> end,
              InterpolablePaletteOperation operation)
      : palette_keyword_(kInterpolablePalette),
        start_(start),
        end_(end),
        operation_(operation) {}
  FontPalette()
      : palette_keyword_(kNormalPalette),
        base_palette_({kNoBasePalette, 0}),
        operation_({kNoInterpolation, 0}) {}

  KeywordPaletteName palette_keyword_;
  AtomicString palette_values_name_;
  BasePaletteValue base_palette_;
  AtomicString match_font_family_;
  Vector<FontPaletteOverride> palette_overrides_;
  scoped_refptr<FontPalette> start_;
  scoped_refptr<FontPalette> end_;
  InterpolablePaletteOperation operation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PALETTE_H_
