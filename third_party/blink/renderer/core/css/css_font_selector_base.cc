// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_selector_base.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

CSSFontSelectorBase::CSSFontSelectorBase(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
#if defined(USE_PARALLEL_TEXT_SHAPING)
    : task_runner_(task_runner)
#endif
{
  DCHECK(IsContextThread());
}

void CSSFontSelectorBase::CountUse(WebFeature feature) const {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (IsContextThread())
    return UseCounter::Count(GetUseCounter(), feature);
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&CSSFontSelectorBase::CountUse,
                          WrapCrossThreadPersistent(this), feature));
#endif
}

AtomicString CSSFontSelectorBase::FamilyNameFromSettings(
    const FontDescription& font_description,
    const FontFamily& generic_family_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsContextThread()) {
    if (IsWebkitBodyFamily(font_description)) {
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(
              &CSSFontSelectorBase::CountUse, WrapCrossThreadPersistent(this),
              WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody));
    }
    return FontSelector::FamilyNameFromSettings(generic_font_family_settings_,
                                                font_description,
                                                generic_family_name, nullptr);
  }
#endif
  return FontSelector::FamilyNameFromSettings(
      generic_font_family_settings_, font_description, generic_family_name,
      GetUseCounter());
}

bool CSSFontSelectorBase::IsContextThread() const {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  return task_runner_->RunsTasksInCurrentSequence();
#else
  return true;
#endif
}

bool CSSFontSelectorBase::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const FontFamily& passed_family) {
  AtomicString family = FamilyNameFromSettings(font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family.FamilyName();
  return FontCache::Get().IsPlatformFamilyMatchAvailable(font_description,
                                                         family);
}

void CSSFontSelectorBase::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportEmojiSegmentGlyphCoverage,
            WrapCrossThreadPersistent(this), num_clusters,
            num_broken_clusters));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportEmojiSegmentGlyphCoverage(
      num_clusters, num_broken_clusters);
}

void CSSFontSelectorBase::ReportFontFamilyLookupByGenericFamily(
    const AtomicString& generic_font_family_name,
    UScriptCode script,
    FontDescription::GenericFamilyType generic_family_type,
    const AtomicString& resulting_font_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportFontFamilyLookupByGenericFamily,
            WrapCrossThreadPersistent(this), generic_font_family_name, script,
            generic_family_type, resulting_font_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFontFamilyLookupByGenericFamily(
      generic_font_family_name, script, generic_family_type,
      resulting_font_name);
}

void CSSFontSelectorBase::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CSSFontSelectorBase::ReportFailedFontFamilyMatch,
                            WrapCrossThreadPersistent(this), font_family_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportSuccessfulFontFamilyMatch(font_family_name);
}

void CSSFontSelectorBase::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CSSFontSelectorBase::ReportFailedFontFamilyMatch,
                            WrapCrossThreadPersistent(this), font_family_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFailedFontFamilyMatch(font_family_name);
}

void CSSFontSelectorBase::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportSuccessfulLocalFontMatch,
            WrapCrossThreadPersistent(this), font_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportSuccessfulLocalFontMatch(font_name);
}

void CSSFontSelectorBase::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CSSFontSelectorBase::ReportFailedLocalFontMatch,
                            WrapCrossThreadPersistent(this), font_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFailedLocalFontMatch(font_name);
}

void CSSFontSelectorBase::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    scoped_refptr<SimpleFontData> resulting_font_data) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportFontLookupByUniqueOrFamilyName,
            WrapCrossThreadPersistent(this), name, font_description,
            resulting_font_data));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      name, font_description, resulting_font_data.get());
}

void CSSFontSelectorBase::ReportFontLookupByUniqueNameOnly(
    const AtomicString& name,
    const FontDescription& font_description,
    scoped_refptr<SimpleFontData> resulting_font_data,
    bool is_loading_fallback) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportFontLookupByUniqueNameOnly,
            WrapCrossThreadPersistent(this), name, font_description,
            resulting_font_data, is_loading_fallback));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFontLookupByUniqueNameOnly(
      name, font_description, resulting_font_data.get(), is_loading_fallback);
}

void CSSFontSelectorBase::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    FontFallbackPriority fallback_priority,
    const FontDescription& font_description,
    scoped_refptr<SimpleFontData> resulting_font_data) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportFontLookupByFallbackCharacter,
            WrapCrossThreadPersistent(this), fallback_character,
            fallback_priority, font_description, resulting_font_data));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportFontLookupByFallbackCharacter(
      fallback_character, fallback_priority, font_description,
      resulting_font_data.get());
}

void CSSFontSelectorBase::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    scoped_refptr<SimpleFontData> resulting_font_data) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &CSSFontSelectorBase::ReportLastResortFallbackFontLookup,
            WrapCrossThreadPersistent(this), font_description,
            resulting_font_data));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportLastResortFallbackFontLookup(
      font_description, resulting_font_data.get());
}

void CSSFontSelectorBase::ReportNotDefGlyph() const {
  CountUse(WebFeature::kFontShapingNotDefGlyphObserved);
}

void CSSFontSelectorBase::ReportSystemFontFamily(
    const AtomicString& font_family_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CSSFontSelectorBase::ReportSystemFontFamily,
                            WrapCrossThreadPersistent(this), font_family_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportSystemFontFamily(font_family_name);
}

void CSSFontSelectorBase::ReportWebFontFamily(
    const AtomicString& font_family_name) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (!IsAlive())
    return;
  if (!IsContextThread()) {
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CSSFontSelectorBase::ReportWebFontFamily,
                            WrapCrossThreadPersistent(this), font_family_name));
    return;
  }
#endif
  GetFontMatchingMetrics()->ReportWebFontFamily(font_family_name);
}

void CSSFontSelectorBase::WillUseFontData(
    const FontDescription& font_description,
    const FontFamily& family,
    const String& text) {
  if (family.FamilyIsGeneric()) {
    if (family.IsPrewarmed() || UNLIKELY(family.FamilyName().IsEmpty()))
      return;
    family.SetIsPrewarmed();
    // |FamilyNameFromSettings| has a visible impact on the load performance.
    // Because |FamilyName.IsPrewarmed| can prevent doing this multiple times
    // only when the |Font| is shared across elements, and therefore it can't
    // help when e.g., the font size is different, check once more if this
    // generic family is already prewarmed.
    {
      AutoLockForParallelTextShaping guard(prewarmed_generic_families_lock_);
      const auto result =
          prewarmed_generic_families_.insert(family.FamilyName());
      if (!result.is_new_entry)
        return;
    }
    const AtomicString& family_name =
        FamilyNameFromSettings(font_description, family);
    if (!family_name.IsEmpty())
      FontCache::PrewarmFamily(family_name);
    return;
  }

  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family.FamilyName())) {
    face->WillUseFontData(font_description, text);
    return;
  }

  if (family.IsPrewarmed() || UNLIKELY(family.FamilyName().IsEmpty()))
    return;
  family.SetIsPrewarmed();
  FontCache::PrewarmFamily(family.FamilyName());
}

void CSSFontSelectorBase::WillUseRange(const FontDescription& font_description,
                                       const AtomicString& family,
                                       const FontDataForRangeSet& range_set) {
  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family))
    face->WillUseRange(font_description, range_set);
}

void CSSFontSelectorBase::Trace(Visitor* visitor) const {
  visitor->Trace(font_face_cache_);
  FontSelector::Trace(visitor);
}

}  // namespace blink
