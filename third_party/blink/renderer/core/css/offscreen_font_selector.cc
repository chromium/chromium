// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

OffscreenFontSelector::OffscreenFontSelector(WorkerGlobalScope* worker)
    : font_face_cache_(MakeGarbageCollected<FontFaceCache>()), worker_(worker) {
  DCHECK(worker);
  FontCache::GetFontCache()->AddClient(this);
}

OffscreenFontSelector::~OffscreenFontSelector() = default;

void OffscreenFontSelector::UpdateGenericFontFamilySettings(
    const GenericFontFamilySettings& settings) {
  generic_font_family_settings_ = settings;
}

void OffscreenFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

void OffscreenFontSelector::UnregisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

scoped_refptr<FontData> OffscreenFontSelector::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family_name) {
  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family_name)) {
    worker_->GetFontMatchingMetrics()->ReportWebFontFamily(family_name);
    return face->GetFontData(font_description);
  }

  worker_->GetFontMatchingMetrics()->ReportSystemFontFamily(family_name);

  // Try to return the correct font based off our settings, in case we were
  // handed the generic font family name.
  AtomicString settings_family_name = FamilyNameFromSettings(
      generic_font_family_settings_, font_description, family_name);
  if (settings_family_name.IsEmpty())
    return nullptr;

  worker_->GetFontMatchingMetrics()->ReportFontFamilyLookupByGenericFamily(
      family_name, font_description.GetScript(),
      font_description.GenericFamily(), settings_family_name);

  auto font_data = FontCache::GetFontCache()->GetFontData(font_description,
                                                          settings_family_name);

  worker_->GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      settings_family_name, font_description, font_data.get());

  return font_data;
}

void OffscreenFontSelector::WillUseFontData(
    const FontDescription& font_description,
    const AtomicString& family,
    const String& text) {
  CSSSegmentedFontFace* face = font_face_cache_->Get(font_description, family);
  if (face)
    face->WillUseFontData(font_description, text);
}

void OffscreenFontSelector::WillUseRange(
    const FontDescription& font_description,
    const AtomicString& family,
    const FontDataForRangeSet& range_set) {
  CSSSegmentedFontFace* face = font_face_cache_->Get(font_description, family);
  if (face)
    face->WillUseRange(font_description, range_set);
}

bool OffscreenFontSelector::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& passed_family) {
  AtomicString family = FamilyNameFromSettings(generic_font_family_settings_,
                                               font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family;
  return FontCache::GetFontCache()->IsPlatformFamilyMatchAvailable(
      font_description, family);
}

void OffscreenFontSelector::ReportNotDefGlyph() const {}

void OffscreenFontSelector::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportEmojiSegmentGlyphCoverage(
      num_clusters, num_broken_clusters);
}

void OffscreenFontSelector::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportSuccessfulFontFamilyMatch(
      font_family_name);
}

void OffscreenFontSelector::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportFailedFontFamilyMatch(
      font_family_name);
}

void OffscreenFontSelector::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportSuccessfulLocalFontMatch(font_name);
}

void OffscreenFontSelector::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportFailedLocalFontMatch(font_name);
}

void OffscreenFontSelector::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      name, font_description, resulting_font_data);
}

void OffscreenFontSelector::ReportFontLookupByUniqueNameOnly(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportFontLookupByUniqueNameOnly(
      name, font_description, resulting_font_data, is_loading_fallback);
}

void OffscreenFontSelector::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    FontFallbackPriority fallback_priority,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportFontLookupByFallbackCharacter(
      fallback_character, fallback_priority, font_description,
      resulting_font_data);
}

void OffscreenFontSelector::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  DCHECK(worker_);
  worker_->GetFontMatchingMetrics()->ReportLastResortFallbackFontLookup(
      font_description, resulting_font_data);
}

void OffscreenFontSelector::FontCacheInvalidated() {
  font_face_cache_->IncrementVersion();
}

void OffscreenFontSelector::FontFaceInvalidated(FontInvalidationReason) {
  FontCacheInvalidated();
}

void OffscreenFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(worker_);
  visitor->Trace(font_face_cache_);
  FontSelector::Trace(visitor);
}

}  // namespace blink
