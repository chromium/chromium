/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_font_selector.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_map.h"
#include "third_party/blink/renderer/platform/fonts/font_matching_metrics.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

CSSFontSelector::CSSFontSelector(const TreeScope& tree_scope)
    : tree_scope_(&tree_scope),
      generic_font_family_settings_(tree_scope.GetDocument()
                                        .GetFrame()
                                        ->GetSettings()
                                        ->GetGenericFontFamilySettings()) {
  DCHECK(tree_scope.GetDocument().GetFrame());
  FontCache::GetFontCache()->AddClient(this);
  if (tree_scope.RootNode().IsDocumentNode()) {
    font_face_cache_ = MakeGarbageCollected<FontFaceCache>();
    FontFaceSetDocument::From(tree_scope.GetDocument())
        ->AddFontFacesToFontFaceCache(font_face_cache_);
  }
}

CSSFontSelector::~CSSFontSelector() = default;

void CSSFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  CHECK(client);
  clients_.insert(client);
}

void CSSFontSelector::UnregisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  clients_.erase(client);
}

void CSSFontSelector::DispatchInvalidationCallbacks(
    FontInvalidationReason reason) {
  font_face_cache_->IncrementVersion();

  HeapVector<Member<FontSelectorClient>> clients;
  CopyToVector(clients_, clients);
  for (auto& client : clients) {
    if (client) {
      client->FontsNeedUpdate(this, reason);
    }
  }
}

void CSSFontSelector::FontFaceInvalidated(FontInvalidationReason reason) {
  DispatchInvalidationCallbacks(reason);
}

void CSSFontSelector::FontCacheInvalidated() {
  DispatchInvalidationCallbacks(FontInvalidationReason::kGeneralInvalidation);
}

scoped_refptr<FontData> CSSFontSelector::GetFontData(
    const FontDescription& font_description,
    const AtomicString& family_name) {
  Document& document = GetTreeScope()->GetDocument();
  if (CSSSegmentedFontFace* face =
          font_face_cache_->Get(font_description, family_name)) {
    document.GetFontMatchingMetrics()->ReportWebFontFamily(family_name);
    return face->GetFontData(font_description);
  }

  document.GetFontMatchingMetrics()->ReportSystemFontFamily(family_name);

  // Try to return the correct font based off our settings, in case we were
  // handed the generic font family name.
  AtomicString settings_family_name = FamilyNameFromSettings(
      generic_font_family_settings_, font_description, family_name);
  if (settings_family_name.IsEmpty())
    return nullptr;

  document.GetFontMatchingMetrics()->ReportFontFamilyLookupByGenericFamily(
      family_name, font_description.GetScript(),
      font_description.GenericFamily(), settings_family_name);

  scoped_refptr<SimpleFontData> font_data =
      FontCache::GetFontCache()->GetFontData(font_description,
                                             settings_family_name);

  document.GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      settings_family_name, font_description, font_data.get());

  return font_data;
}

void CSSFontSelector::WillUseFontData(const FontDescription& font_description,
                                      const AtomicString& family,
                                      const String& text) {
  CSSSegmentedFontFace* face = font_face_cache_->Get(font_description, family);
  if (face)
    face->WillUseFontData(font_description, text);
}

void CSSFontSelector::WillUseRange(const FontDescription& font_description,
                                   const AtomicString& family,
                                   const FontDataForRangeSet& range_set) {
  CSSSegmentedFontFace* face = font_face_cache_->Get(font_description, family);
  if (face)
    face->WillUseRange(font_description, range_set);
}

bool CSSFontSelector::IsPlatformFamilyMatchAvailable(
    const FontDescription& font_description,
    const AtomicString& passed_family) {
  AtomicString family = FamilyNameFromSettings(generic_font_family_settings_,
                                               font_description, passed_family);
  if (family.IsEmpty())
    family = passed_family;
  return FontCache::GetFontCache()->IsPlatformFamilyMatchAvailable(
      font_description, family);
}

void CSSFontSelector::UpdateGenericFontFamilySettings(Document& document) {
  if (!document.GetSettings())
    return;
  generic_font_family_settings_ =
      document.GetSettings()->GetGenericFontFamilySettings();
  FontCacheInvalidated();
}

void CSSFontSelector::ReportNotDefGlyph() const {
  UseCounter::Count(GetDocument(), WebFeature::kFontShapingNotDefGlyphObserved);
}

void CSSFontSelector::ReportEmojiSegmentGlyphCoverage(
    unsigned num_clusters,
    unsigned num_broken_clusters) {
  GetDocument().GetFontMatchingMetrics()->ReportEmojiSegmentGlyphCoverage(
      num_clusters, num_broken_clusters);
}

void CSSFontSelector::ReportSuccessfulFontFamilyMatch(
    const AtomicString& font_family_name) {
  GetDocument().GetFontMatchingMetrics()->ReportSuccessfulFontFamilyMatch(
      font_family_name);
}

void CSSFontSelector::ReportFailedFontFamilyMatch(
    const AtomicString& font_family_name) {
  GetDocument().GetFontMatchingMetrics()->ReportFailedFontFamilyMatch(
      font_family_name);
}

void CSSFontSelector::ReportSuccessfulLocalFontMatch(
    const AtomicString& font_name) {
  GetDocument().GetFontMatchingMetrics()->ReportSuccessfulLocalFontMatch(
      font_name);
}

void CSSFontSelector::ReportFailedLocalFontMatch(
    const AtomicString& font_name) {
  GetDocument().GetFontMatchingMetrics()->ReportFailedLocalFontMatch(font_name);
}

void CSSFontSelector::ReportFontLookupByUniqueOrFamilyName(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetDocument().GetFontMatchingMetrics()->ReportFontLookupByUniqueOrFamilyName(
      name, font_description, resulting_font_data);
}

void CSSFontSelector::ReportFontLookupByUniqueNameOnly(
    const AtomicString& name,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data,
    bool is_loading_fallback) {
  GetDocument().GetFontMatchingMetrics()->ReportFontLookupByUniqueNameOnly(
      name, font_description, resulting_font_data, is_loading_fallback);
}

void CSSFontSelector::ReportFontLookupByFallbackCharacter(
    UChar32 fallback_character,
    FontFallbackPriority fallback_priority,
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetDocument().GetFontMatchingMetrics()->ReportFontLookupByFallbackCharacter(
      fallback_character, fallback_priority, font_description,
      resulting_font_data);
}

void CSSFontSelector::ReportLastResortFallbackFontLookup(
    const FontDescription& font_description,
    SimpleFontData* resulting_font_data) {
  GetDocument().GetFontMatchingMetrics()->ReportLastResortFallbackFontLookup(
      font_description, resulting_font_data);
}

void CSSFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(font_face_cache_);
  visitor->Trace(clients_);
  FontSelector::Trace(visitor);
}

}  // namespace blink
