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
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"

namespace blink {

CSSFontSelector::CSSFontSelector(const TreeScope& tree_scope)
    : tree_scope_(&tree_scope) {
  DCHECK(tree_scope.GetDocument().GetExecutionContext()->IsContextThread());
  DCHECK(tree_scope.GetDocument().GetFrame());
  generic_font_family_settings_ = tree_scope.GetDocument()
                                      .GetFrame()
                                      ->GetSettings()
                                      ->GetGenericFontFamilySettings();
  FontCache::Get().AddClient(this);
  if (tree_scope.RootNode().IsDocumentNode()) {
    font_face_cache_ = MakeGarbageCollected<FontFaceCache>();
    FontFaceSetDocument::From(tree_scope.GetDocument())
        ->AddFontFacesToFontFaceCache(font_face_cache_);
  }
}

CSSFontSelector::~CSSFontSelector() = default;

UseCounter* CSSFontSelector::GetUseCounter() const {
  auto* const context = GetExecutionContext();
  return context && context->IsContextThread() ? context : nullptr;
}

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

  HeapVector<Member<FontSelectorClient>> clients(clients_);
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
    const FontFamily& font_family) {
  const auto& family_name = font_family.FamilyName();
  Document& document = GetTreeScope()->GetDocument();

  FontDescription request_description(font_description);
  FontPalette* request_palette = request_description.GetFontPalette();

  if (request_palette && request_palette->IsCustomPalette()) {
    AtomicString requested_palette_values =
        request_palette->GetPaletteValuesName();
    StyleRuleFontPaletteValues* font_palette_values =
        document.GetStyleEngine().FontPaletteValuesForNameAndFamily(
            requested_palette_values, family_name);
    if (font_palette_values) {
      scoped_refptr<FontPalette> new_request_palette =
          FontPalette::Create(requested_palette_values);
      new_request_palette->SetMatchFamilyName(
          font_palette_values->GetFontFamilyAsString());
      new_request_palette->SetBasePalette(
          font_palette_values->GetBasePaletteIndex());
      Vector<FontPalette::FontPaletteOverride> override_colors =
          font_palette_values->GetOverrideColorsAsVector();
      if (override_colors.size()) {
        new_request_palette->SetColorOverrides(std::move(override_colors));
      }
      request_description.SetFontPalette(new_request_palette);
    }
  }

  if (request_description.GetFontVariantAlternates()) {
    // TODO(https://crbug.com/1382722): For scoping to work correctly, we'd need
    // to traverse the TreeScopes here and fuse / override values of
    // @font-feature-values from these.
    const FontFeatureValuesStorage* feature_values_storage =
        document.GetScopedStyleResolver()
            ? document.GetScopedStyleResolver()->FontFeatureValuesForFamily(
                  family_name)
            : nullptr;
    scoped_refptr<FontVariantAlternates> new_alternates = nullptr;
    if (feature_values_storage) {
      new_alternates = request_description.GetFontVariantAlternates()->Resolve(
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveStylistic(alias);
          },
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveStyleset(alias);
          },
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveCharacterVariant(alias);
          },
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveSwash(alias);
          },
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveOrnaments(alias);
          },
          [feature_values_storage](AtomicString alias) {
            return feature_values_storage->ResolveAnnotation(alias);
          });
    } else {
      // If no StyleRuleFontFeature alias table values for this font was found,
      // it still needs a resolve call to convert historical-forms state (which
      // is not looked-up against StyleRuleFontFeatureValues) to an internal
      // feature.
      auto no_lookup = [](AtomicString) -> Vector<uint32_t> { return {}; };
      new_alternates = request_description.GetFontVariantAlternates()->Resolve(
          no_lookup, no_lookup, no_lookup, no_lookup, no_lookup, no_lookup);
    }

    if (new_alternates) {
      request_description.SetFontVariantAlternates(new_alternates);
    }
  }

  if (!font_family.FamilyIsGeneric()) {
    if (CSSSegmentedFontFace* face =
            font_face_cache_->Get(request_description, family_name)) {
      ReportWebFontFamily(family_name);
      return face->GetFontData(request_description);
    }
  }

  ReportSystemFontFamily(family_name);

  // Try to return the correct font based off our settings, in case we were
  // handed the generic font family name.
  AtomicString settings_family_name =
      FamilyNameFromSettings(request_description, font_family);
  if (settings_family_name.empty()) {
    return nullptr;
  }

  ReportFontFamilyLookupByGenericFamily(
      family_name, request_description.GetScript(),
      request_description.GenericFamily(), settings_family_name);

  scoped_refptr<SimpleFontData> font_data =
      FontCache::Get().GetFontData(request_description, settings_family_name);

  ReportFontLookupByUniqueOrFamilyName(settings_family_name,
                                       request_description, font_data);

  return font_data;
}

void CSSFontSelector::UpdateGenericFontFamilySettings(Document& document) {
  if (!document.GetSettings()) {
    return;
  }
  generic_font_family_settings_ =
      document.GetSettings()->GetGenericFontFamilySettings();
  FontCacheInvalidated();
}

FontMatchingMetrics* CSSFontSelector::GetFontMatchingMetrics() const {
  return GetDocument().GetFontMatchingMetrics();
}

bool CSSFontSelector::IsAlive() const {
  return tree_scope_;
}

void CSSFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(clients_);
  CSSFontSelectorBase::Trace(visitor);
}

}  // namespace blink
