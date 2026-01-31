// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_COLOR_PALETTE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_COLOR_PALETTE_UTIL_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/memory/raw_ptr.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/color/dynamic_color/palette_factory.h"

// A block type that provides a dynamic color based on the current trait
// collection.
typedef UIColor* (^DynamicColorProviderBlock)(UITraitCollection* traits);

// Represents a tone value tied to a specific TonalPalette.
struct PaletteTone {
  // A pointer to the immutable source palette.
  raw_ptr<const ui::TonalPalette> palette;
  // The specific tone value from the palette.
  int tone;

  // Binds a palette reference and tone value.
  PaletteTone(const ui::TonalPalette& associatedPalette, int toneValue);
  // Resolves the ARGB SkColor from the palette for this tone.
  SkColor color() const;
};

// Represents a color input that can be either a palette-based tone or a fixed
// UIColor. Only one of `tone` or `fixedColor` should be set.
struct DynamicColorInput {
  std::optional<PaletteTone> tone;
  std::optional<UIColor*> fixedColor;

  explicit DynamicColorInput(const PaletteTone& t);
  explicit DynamicColorInput(UIColor* color);
  ~DynamicColorInput();  // Declare destructor

  // Resolves the input to a specific SkColor.
  SkColor resolveColor() const;
};

// Factory helper for creating DynamicColorInput from a PaletteTone.
inline DynamicColorInput FromTone(const PaletteTone& tone) {
  return DynamicColorInput(tone);
}

// Factory helper for creating DynamicColorInput from a fixed UIColor.
inline DynamicColorInput FromColor(UIColor* color) {
  return DynamicColorInput(color);
}

// Returns a block that provides a dynamic color based on the trait collection.
DynamicColorProviderBlock GetDynamicProvider(
    const DynamicColorInput& lightInput,
    const DynamicColorInput& darkInput);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_COLOR_PALETTE_COLOR_PALETTE_UTIL_H_
