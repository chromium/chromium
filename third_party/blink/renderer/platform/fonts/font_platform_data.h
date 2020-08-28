/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PLATFORM_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_PLATFORM_DATA_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

#if defined(OS_MAC)
typedef const struct __CTFont* CTFontRef;
#endif  // defined(OS_MAC)

class SkFont;
class SkTypeface;
typedef uint32_t SkFontID;

namespace blink {

class Font;
class HarfBuzzFace;

class PLATFORM_EXPORT FontPlatformData {
  USING_FAST_MALLOC(FontPlatformData);

 public:
  // Used for deleted values in the font cache's hash tables. The hash table
  // will create us with this structure, and it will compare other values
  // to this "Deleted" one. It expects the Deleted one to be differentiable
  // from the 0 one (created with the empty constructor), so we can't just
  // set everything to 0.
  FontPlatformData(WTF::HashTableDeletedValueType);
  FontPlatformData();
  FontPlatformData(const FontPlatformData&);
  FontPlatformData(float size,
                   bool synthetic_bold,
                   bool synthetic_italic,
                   FontOrientation = FontOrientation::kHorizontal);
  FontPlatformData(const FontPlatformData& src, float text_size);
  FontPlatformData(const sk_sp<SkTypeface>,
                   const std::string& name,
                   float text_size,
                   bool synthetic_bold,
                   bool synthetic_italic,
                   FontOrientation = FontOrientation::kHorizontal);
  ~FontPlatformData();

#if defined(OS_MAC)
  // Returns nullptr for FreeType backed SkTypefaces, compare
  // FontCustomPlatformData, which are used for variable fonts on Mac OS
  // <10.12. It should not return nullptr otherwise. So it allows distinguishing
  // which backend the SkTypeface is using.
  CTFontRef CtFont() const;
#endif

  String FontFamilyName() const;
  float size() const { return text_size_; }
  bool SyntheticBold() const { return synthetic_bold_; }
  bool SyntheticItalic() const { return synthetic_italic_; }

  SkTypeface* Typeface() const;
  HarfBuzzFace* GetHarfBuzzFace() const;
  bool HasSpaceInLigaturesOrKerning(TypesettingFeatures) const;
  SkFontID UniqueID() const;
  unsigned GetHash() const;

  FontOrientation Orientation() const { return orientation_; }
  bool IsVerticalAnyUpright() const {
    return blink::IsVerticalAnyUpright(orientation_);
  }
  void SetOrientation(FontOrientation orientation) {
    orientation_ = orientation;
  }
  void SetSyntheticBold(bool synthetic_bold) {
    synthetic_bold_ = synthetic_bold;
  }
  void SetSyntheticItalic(bool synthetic_italic) {
    synthetic_italic_ = synthetic_italic;
  }
  void SetAvoidEmbeddedBitmaps(bool embedded_bitmaps) {
    avoid_embedded_bitmaps_ = embedded_bitmaps;
  }
  bool operator==(const FontPlatformData&) const;
  const FontPlatformData& operator=(const FontPlatformData&);

  bool IsHashTableDeletedValue() const { return is_hash_table_deleted_value_; }
  bool FontContainsCharacter(UChar32 character);

#if !defined(OS_WIN) && !defined(OS_MAC)
  const WebFontRenderStyle& GetFontRenderStyle() const { return style_; }
#endif

  void SetupSkFont(SkFont*,
                   float device_scale_factor = 1,
                   const Font* = nullptr) const;

  // Computes a digest from the typeface. The digest only depends on the
  // underlying font itself, and does not vary by the style (size, weight,
  // italics, etc). This is aimed at discovering the fingerprinting information
  // a particular local font may provide websites.
  //
  // The digest algorithm is designed for fast computation, rather than to be
  // robust against an attacker with control of local fonts looking to attack
  // the fingerprinting algorithm.
  IdentifiableToken ComputeTypefaceDigest() const;

 private:
#if !defined(OS_WIN) && !defined(OS_MAC)
  WebFontRenderStyle QuerySystemRenderStyle(const std::string& family,
                                            float text_size,
                                            SkFontStyle);
#endif
#if defined(OS_WIN)
  // TODO(https://crbug.com/808221): Remove and use QuerySystemRenderStyle()
  // instead.
  WebFontRenderStyle QuerySystemForRenderStyle();
#endif

  sk_sp<SkTypeface> typeface_;
#if !defined(OS_WIN) && !defined(OS_MAC)
  std::string family_;
#endif

 public:
  float text_size_;
  bool synthetic_bold_;
  bool synthetic_italic_;
  bool avoid_embedded_bitmaps_;
  FontOrientation orientation_;

 private:
#if !defined(OS_MAC)
  WebFontRenderStyle style_;
#endif

  mutable scoped_refptr<HarfBuzzFace> harfbuzz_face_;
  bool is_hash_table_deleted_value_;
};

}  // namespace blink

#endif  // ifdef FontPlatformData_h
