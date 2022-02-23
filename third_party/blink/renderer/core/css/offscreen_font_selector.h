// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OFFSCREEN_FONT_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OFFSCREEN_FONT_SELECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_font_selector_base.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ExecutionContext;
class FontDescription;
class FontFamily;

class CORE_EXPORT OffscreenFontSelector : public CSSFontSelectorBase {
 public:
  explicit OffscreenFontSelector(WorkerGlobalScope*);
  ~OffscreenFontSelector() override;

  unsigned Version() const override { return 1; }

  void ReportSuccessfulFontFamilyMatch(
      const AtomicString& font_family_name) override;

  void ReportFailedFontFamilyMatch(
      const AtomicString& font_family_name) override;

  void ReportSuccessfulLocalFontMatch(const AtomicString& font_name) override;

  void ReportFailedLocalFontMatch(const AtomicString& font_name) override;

  void ReportFontLookupByUniqueOrFamilyName(
      const AtomicString& name,
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data) override;

  void ReportFontLookupByUniqueNameOnly(
      const AtomicString& name,
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data,
      bool is_loading_fallback = false) override;

  void ReportFontLookupByFallbackCharacter(
      UChar32 fallback_character,
      FontFallbackPriority fallback_priority,
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data) override;

  void ReportLastResortFallbackFontLookup(
      const FontDescription& font_description,
      SimpleFontData* resulting_font_data) override;

  void ReportNotDefGlyph() const override;

  void ReportEmojiSegmentGlyphCoverage(unsigned num_clusters,
                                       unsigned num_broken_clusters) override;

  scoped_refptr<FontData> GetFontData(const FontDescription&,
                                      const FontFamily&) override;

  void RegisterForInvalidationCallbacks(FontSelectorClient*) override;
  void UnregisterForInvalidationCallbacks(FontSelectorClient*) override;

  const GenericFontFamilySettings& GetGenericFontFamilySettings() const {
    return generic_font_family_settings_;
  }

  void FontCacheInvalidated() override;
  void FontFaceInvalidated(FontInvalidationReason) override;

  void UpdateGenericFontFamilySettings(const GenericFontFamilySettings&);

  FontFaceCache* GetFontFaceCache() override { return font_face_cache_; }

  ExecutionContext* GetExecutionContext() const override {
    return worker_ ? worker_->GetExecutionContext() : nullptr;
  }

  void Trace(Visitor*) const override;

 protected:
  UseCounter* GetUseCounter() override;
  void DispatchInvalidationCallbacks();

 private:
  Member<WorkerGlobalScope> worker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_OFFSCREEN_FONT_SELECTOR_H_
