// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"

namespace blink {

OffscreenFontSelector::OffscreenFontSelector(WorkerGlobalScope* worker)
    : worker_(worker) {
  DCHECK(worker);
  font_face_cache_ = MakeGarbageCollected<FontFaceCache>();
  FontCache::Get().AddClient(this);
}

OffscreenFontSelector::~OffscreenFontSelector() = default;

FontMatchingMetrics* OffscreenFontSelector::GetFontMatchingMetrics() const {
  return worker_->GetFontMatchingMetrics();
}

UseCounter* OffscreenFontSelector::GetUseCounter() const {
  return GetExecutionContext();
}

void OffscreenFontSelector::UpdateGenericFontFamilySettings(
    const GenericFontFamilySettings& settings) {
  generic_font_family_settings_ = settings;
}

void OffscreenFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

void OffscreenFontSelector::UnregisterForInvalidationCallbacks(
    FontSelectorClient* client) {}

const FontData* OffscreenFontSelector::GetFontData(
    const FontDescription& font_description,
    const FontFamily& font_family) {
  const auto& family_name = font_family.FamilyName();
  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family_name)) {
    return face->GetFontData(font_description);
  }

  // Try to return the correct font based off our settings, in case we were
  // handed the generic font family name.
  AtomicString settings_family_name =
      FamilyNameFromSettings(font_description, font_family);
  if (settings_family_name.empty()) {
    return nullptr;
  }

  ReportFontFamilyLookupByGenericFamily(
      family_name, font_description.GetScript(),
      font_description.GenericFamily(), settings_family_name);

  const auto* font_data =
      FontCache::Get().GetFontData(font_description, settings_family_name);

  ReportFontLookupByUniqueOrFamilyName(settings_family_name, font_description,
                                       font_data);

  return font_data;
}

void OffscreenFontSelector::FontCacheInvalidated() {
  font_face_cache_->IncrementVersion();
}

void OffscreenFontSelector::FontFaceInvalidated(FontInvalidationReason) {
  FontCacheInvalidated();
}

void OffscreenFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(worker_);
  CSSFontSelectorBase::Trace(visitor);
}

}  // namespace blink
