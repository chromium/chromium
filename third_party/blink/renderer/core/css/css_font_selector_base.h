// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_SELECTOR_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_SELECTOR_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FontDescription;
class FontFamily;

// `CSSFontSelectorBase` is the base class of CSS related font selectors:
//  * `CSSFontSelector` for `StyleEngine`
//  * `PopupMenuCSSFontSelector` derived from `CSSFontSelector`
//  * `OffscreenFontSelector` for `WorkerGlobalScope`.
class CORE_EXPORT CSSFontSelectorBase : public FontSelector {
 public:
  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const FontFamily& family) override;

  void WillUseFontData(const FontDescription&,
                       const FontFamily& family,
                       const String& text) override;
  void WillUseRange(const FontDescription&,
                    const AtomicString& family_name,
                    const FontDataForRangeSet&) override;

  void ReportNotDefGlyph() const override;

  void Trace(Visitor*) const override;

 protected:
  // TODO(crbug.com/383860): We should get rid of `IsAlive()` once lifetime
  // issue of `CSSFontSelector` is solved. It will be alive after `TreeScope`
  // is dead.
  virtual bool IsAlive() const { return true; }

  virtual UseCounter* GetUseCounter() const = 0;

  AtomicString FamilyNameFromSettings(const FontDescription&,
                                      const FontFamily& generic_family_name);
  Member<FontFaceCache> font_face_cache_;
  GenericFontFamilySettings generic_font_family_settings_;
  HashSet<AtomicString> prewarmed_generic_families_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_SELECTOR_BASE_H_
