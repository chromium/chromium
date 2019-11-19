/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"

#import <AppKit/NSFont.h>
#import <AvailabilityMacros.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#import "third_party/blink/public/platform/mac/web_sandbox_support.h"
#import "third_party/blink/public/platform/platform.h"
#import "third_party/blink/renderer/platform/fonts/font.h"
#import "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#import "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#import "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#import "third_party/blink/renderer/platform/web_test_support.h"
#import "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#import "third_party/skia/include/core/SkFont.h"
#import "third_party/skia/include/core/SkStream.h"
#import "third_party/skia/include/ports/SkTypeface_mac.h"

namespace blink {

static bool CanLoadInProcess(NSFont* ns_font) {
  base::ScopedCFTypeRef<CGFontRef> cg_font(
      CTFontCopyGraphicsFont(base::mac::NSToCFCast(ns_font), 0));
  // Toll-free bridged types CFStringRef and NSString*.
  base::scoped_nsobject<NSString> font_name(
      base::mac::CFToNSCast(CGFontCopyPostScriptName(cg_font)));
  return ![font_name isEqualToString:@"LastResort"];
}

static CTFontDescriptorRef CascadeToLastResortFontDescriptor() {
  static CTFontDescriptorRef descriptor;
  if (descriptor)
    return descriptor;

  base::ScopedCFTypeRef<CTFontDescriptorRef> last_resort(
      CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));
  const void* descriptors[] = {last_resort};
  base::ScopedCFTypeRef<CFArrayRef> values_array(
      CFArrayCreate(kCFAllocatorDefault, descriptors, base::size(descriptors),
                    &kCFTypeArrayCallBacks));

  const void* keys[] = {kCTFontCascadeListAttribute};
  const void* values[] = {values_array};
  base::ScopedCFTypeRef<CFDictionaryRef> attributes(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, base::size(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  descriptor = CTFontDescriptorCreateWithAttributes(attributes);

  return descriptor;
}

static sk_sp<SkTypeface> LoadFromBrowserProcess(NSFont* ns_font,
                                                float text_size) {
  // Send cross-process request to load font.
  WebSandboxSupport* sandbox_support = Platform::Current()->GetSandboxSupport();
  if (!sandbox_support) {
    // This function should only be called in response to an error loading a
    // font due to being blocked by the sandbox.
    // This by definition shouldn't happen if there is no sandbox support.
    NOTREACHED();
    return nullptr;
  }

  CGFontRef loaded_cg_font;
  uint32_t font_id;
  if (!sandbox_support->LoadFont(base::mac::NSToCFCast(ns_font),
                                 &loaded_cg_font, &font_id)) {
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Loading user font \"" << [[ns_font familyName] UTF8String]
        << "\" from non system location failed. Corrupt or missing font file?";
    return nullptr;
  }
  base::ScopedCFTypeRef<CGFontRef> cg_font(loaded_cg_font);
  base::ScopedCFTypeRef<CTFontRef> ct_font(CTFontCreateWithGraphicsFont(
      cg_font, text_size, 0, CascadeToLastResortFontDescriptor()));
  sk_sp<SkTypeface> return_font(SkCreateTypefaceFromCTFont(ct_font, cg_font));

  if (!return_font.get())
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Instantiating SkTypeface from user font failed for font family \""
        << [[ns_font familyName] UTF8String] << "\".";
  return return_font;
}

std::unique_ptr<FontPlatformData> FontPlatformDataFromNSFont(
    NSFont* ns_font,
    float size,
    bool synthetic_bold,
    bool synthetic_italic,
    FontOrientation orientation,
    FontVariationSettings* variation_settings) {
  DCHECK(ns_font);
  sk_sp<SkTypeface> typeface;
  if (CanLoadInProcess(ns_font)) {
    typeface.reset(SkCreateTypefaceFromCTFont(base::mac::NSToCFCast(ns_font)));
  } else {
    // In process loading fails for cases where third party font manager
    // software registers fonts in non system locations such as /Library/Fonts
    // and ~/Library Fonts, see crbug.com/72727 or crbug.com/108645.
    typeface = LoadFromBrowserProcess(ns_font, size);
  }

  if (variation_settings && variation_settings->size() < UINT16_MAX) {
    SkFontArguments::Axis axes[variation_settings->size()];
    for (size_t i = 0; i < variation_settings->size(); ++i) {
      AtomicString feature_tag = variation_settings->at(i).Tag();
      axes[i] = {AtomicStringToFourByteTag(feature_tag),
                 SkFloatToScalar(variation_settings->at(i).Value())};
    }
    sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
    // TODO crbug.com/670246: Refactor this to a future Skia API that acccepts
    // axis parameters on system fonts directly.
    typeface = fm->makeFromStream(
        typeface->openStream(nullptr)->duplicate(),
        SkFontArguments().setAxes(axes, variation_settings->size()));
  }

  return std::make_unique<FontPlatformData>(
      std::move(typeface),
      std::string(),  // family_ doesn't exist on Mac, this avoids conversion
                      // from NSString which requires including a //base header
      size, synthetic_bold, synthetic_italic, orientation);
}

void FontPlatformData::SetupSkFont(SkFont* skfont,
                                   float,
                                   const Font* font) const {
  bool should_smooth_fonts = true;
  bool should_antialias = true;
  bool should_subpixel_position = true;

  if (font) {
    switch (font->GetFontDescription().FontSmoothing()) {
      case kAntialiased:
        should_smooth_fonts = false;
        break;
      case kSubpixelAntialiased:
        break;
      case kNoSmoothing:
        should_antialias = false;
        should_smooth_fonts = false;
        break;
      case kAutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default
        // settings.
        break;
    }
  }

  if (WebTestSupport::IsRunningWebTest()) {
    should_smooth_fonts = false;
    should_antialias =
        should_antialias && WebTestSupport::IsFontAntialiasingEnabledForTest();
    should_subpixel_position =
        WebTestSupport::IsTextSubpixelPositioningAllowedForTest();
  }

  if (should_antialias && should_smooth_fonts) {
    skfont->setEdging(SkFont::Edging::kSubpixelAntiAlias);
  } else if (should_antialias) {
    skfont->setEdging(SkFont::Edging::kAntiAlias);
  } else {
    skfont->setEdging(SkFont::Edging::kAlias);
  }
  skfont->setEmbeddedBitmaps(false);
  const float ts = text_size_ >= 0 ? text_size_ : 12;
  skfont->setSize(SkFloatToScalar(ts));
  skfont->setTypeface(typeface_);
  skfont->setEmbolden(synthetic_bold_);
  skfont->setSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);
  skfont->setSubpixel(should_subpixel_position);

  // CoreText always provides linear metrics if it can, so the linear metrics
  // flag setting doesn't affect typefaces backed by CoreText. However, it
  // does affect FreeType backed typefaces, so set the flag for consistency.
  skfont->setLinearMetrics(should_subpixel_position);

  // When rendering using CoreGraphics, disable hinting when
  // webkit-font-smoothing:antialiased or text-rendering:geometricPrecision is
  // used.  See crbug.com/152304
  if (font &&
      (font->GetFontDescription().FontSmoothing() == kAntialiased ||
       font->GetFontDescription().TextRendering() == kGeometricPrecision))
    skfont->setHinting(SkFontHinting::kNone);
}

}  // namespace blink
