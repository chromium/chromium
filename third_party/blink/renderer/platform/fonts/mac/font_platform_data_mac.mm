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

#import "third_party/blink/renderer/platform/fonts/font_platform_data.h"

#import <AppKit/NSFont.h>
#import <AvailabilityMacros.h>
#import "third_party/blink/public/platform/mac/web_sandbox_support.h"
#import "third_party/blink/public/platform/platform.h"
#import "third_party/blink/renderer/platform/fonts/font.h"
#import "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#import "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#import "third_party/blink/renderer/platform/layout_test_support.h"
#import "third_party/blink/renderer/platform/wtf/retain_ptr.h"
#import "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#import "third_party/skia/include/core/SkStream.h"
#import "third_party/skia/include/ports/SkTypeface_mac.h"

namespace blink {

static bool CanLoadInProcess(NSFont* ns_font) {
  RetainPtr<CGFontRef> cg_font(kAdoptCF,
                               CTFontCopyGraphicsFont(toCTFontRef(ns_font), 0));
  // Toll-free bridged types CFStringRef and NSString*.
  RetainPtr<NSString> font_name(
      kAdoptNS, const_cast<NSString*>(reinterpret_cast<const NSString*>(
                    CGFontCopyPostScriptName(cg_font.Get()))));
  return ![font_name.Get() isEqualToString:@"LastResort"];
}

static CTFontDescriptorRef CascadeToLastResortFontDescriptor() {
  static CTFontDescriptorRef descriptor;
  if (descriptor)
    return descriptor;

  RetainPtr<CTFontDescriptorRef> last_resort(
      kAdoptCF, CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));
  const void* descriptors[] = {last_resort.Get()};
  RetainPtr<CFArrayRef> values_array(
      kAdoptCF, CFArrayCreate(kCFAllocatorDefault, descriptors,
                              arraysize(descriptors), &kCFTypeArrayCallBacks));

  const void* keys[] = {kCTFontCascadeListAttribute};
  const void* values[] = {values_array.Get()};
  RetainPtr<CFDictionaryRef> attributes(
      kAdoptCF,
      CFDictionaryCreate(kCFAllocatorDefault, keys, values, arraysize(keys),
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));

  descriptor = CTFontDescriptorCreateWithAttributes(attributes.Get());

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
  if (!sandbox_support->LoadFont(toCTFontRef(ns_font), &loaded_cg_font,
                                 &font_id)) {
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Loading user font \"" << [[ns_font familyName] UTF8String]
        << "\" from non system location failed. Corrupt or missing font file?";
    return nullptr;
  }
  RetainPtr<CGFontRef> cg_font(kAdoptCF, loaded_cg_font);
  RetainPtr<CTFontRef> ct_font(
      kAdoptCF,
      CTFontCreateWithGraphicsFont(cg_font.Get(), text_size, 0,
                                   CascadeToLastResortFontDescriptor()));
  sk_sp<SkTypeface> return_font(
      SkCreateTypefaceFromCTFont(ct_font.Get(), cg_font.Get()));

  if (!return_font.get())
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Instantiating SkTypeface from user font failed for font family \""
        << [[ns_font familyName] UTF8String] << "\".";
  return return_font;
}

void FontPlatformData::SetupSkPaint(SkPaint* paint,
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

  if (LayoutTestSupport::IsRunningLayoutTest()) {
    should_smooth_fonts = false;
    should_antialias = should_antialias &&
                       LayoutTestSupport::IsFontAntialiasingEnabledForTest();
    should_subpixel_position =
        LayoutTestSupport::IsTextSubpixelPositioningAllowedForTest();
  }

  paint->setAntiAlias(should_antialias);
  paint->setEmbeddedBitmapText(false);
  const float ts = text_size_ >= 0 ? text_size_ : 12;
  paint->setTextSize(SkFloatToScalar(ts));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText(synthetic_bold_);
  paint->setTextSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);
  paint->setLCDRenderText(should_smooth_fonts);
  paint->setSubpixelText(should_subpixel_position);

  // When rendering using CoreGraphics, disable hinting when
  // webkit-font-smoothing:antialiased or text-rendering:geometricPrecision is
  // used.  See crbug.com/152304
  if (font &&
      (font->GetFontDescription().FontSmoothing() == kAntialiased ||
       font->GetFontDescription().TextRendering() == kGeometricPrecision))
    paint->setHinting(SkPaint::kNo_Hinting);
}

FontPlatformData::FontPlatformData(NSFont* ns_font,
                                   float size,
                                   bool synthetic_bold,
                                   bool synthetic_italic,
                                   FontOrientation orientation,
                                   FontVariationSettings* variation_settings)
    : text_size_(size),
      synthetic_bold_(synthetic_bold),
      synthetic_italic_(synthetic_italic),
      orientation_(orientation),
      is_hash_table_deleted_value_(false) {
  DCHECK(ns_font);
  sk_sp<SkTypeface> typeface;
  if (CanLoadInProcess(ns_font)) {
    typeface.reset(SkCreateTypefaceFromCTFont(toCTFontRef(ns_font)));
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
  typeface_ = typeface;
}

}  // namespace blink
