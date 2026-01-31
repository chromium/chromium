// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/color_palette/color_palette_util.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "skia/ext/skia_utils_ios.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/color/dynamic_color/palette_factory.h"

// PaletteTone definitions.
PaletteTone::PaletteTone(const ui::TonalPalette& associatedPalette,
                         int toneValue)
    : palette(&associatedPalette), tone(toneValue) {}

SkColor PaletteTone::color() const {
  return palette->get(tone);
}

// DynamicColorInput definitions.
DynamicColorInput::DynamicColorInput(const PaletteTone& t) : tone(t) {}

DynamicColorInput::DynamicColorInput(UIColor* color) : fixedColor(color) {}
DynamicColorInput::~DynamicColorInput() =
    default;  // Define destructor for Google style.

SkColor DynamicColorInput::resolveColor() const {
  if (tone.has_value()) {
    return tone->color();
  }
  if (fixedColor.has_value()) {
    return skia::UIColorToSkColor(fixedColor.value());
  }
  return SK_ColorBLACK;
}

// GetDynamicProvider definition.
DynamicColorProviderBlock GetDynamicProvider(
    const DynamicColorInput& lightInput,
    const DynamicColorInput& darkInput) {
  SkColor lightSkColor = lightInput.resolveColor();
  SkColor darkSkColor = darkInput.resolveColor();

  return ^UIColor*(UITraitCollection* traits) {
    BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
    return skia::UIColorFromSkColor(isDark ? darkSkColor : lightSkColor);
  };
}
