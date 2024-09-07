// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/local_font_face_source.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_custom_font_data.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

LocalFontFaceSource::LocalFontFaceSource(CSSFontFace* css_font_face,
                                         FontSelector* font_selector,
                                         const String& font_name)
    : face_(css_font_face),
      font_selector_(font_selector),
      font_name_(font_name) {}

LocalFontFaceSource::~LocalFontFaceSource() {}

bool LocalFontFaceSource::IsLocalNonBlocking() const {
  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get().GetFontUniqueNameLookup();
  if (!unique_name_lookup) {
    return true;
  }
  return unique_name_lookup->IsFontUniqueNameLookupReadyForSyncLookup();
}

bool LocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) const {
  // TODO(crbug.com/1027158): Remove metrics code after metrics collected.
  // TODO(crbug.com/1025945): Properly handle Windows prior to 10 and Android.
  bool font_available = FontCache::Get().IsPlatformFontUniqueNameMatchAvailable(
      font_description, font_name_);
  if (font_available) {
    font_selector_->ReportSuccessfulLocalFontMatch(font_name_);
  } else {
    font_selector_->ReportFailedLocalFontMatch(font_name_);
  }
  return font_available;
}

const SimpleFontData* LocalFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  FontCachePurgePreventer font_cache_purge_preventer;
  const SimpleFontData* temporary_font =
      FontCache::Get().GetLastResortFallbackFont(font_description);
  if (!temporary_font) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  CSSCustomFontData* css_font_data = MakeGarbageCollected<CSSCustomFontData>(
      this, CSSCustomFontData::kVisibleFallback);
  return MakeGarbageCollected<SimpleFontData>(&temporary_font->PlatformData(),
                                              css_font_data);
}

const SimpleFontData* LocalFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  if (!IsValid()) {
    ReportFontLookup(font_description, nullptr);
    return nullptr;
  }

  bool local_fonts_enabled = true;
  probe::LocalFontsEnabled(font_selector_->GetExecutionContext(),
                           &local_fonts_enabled);

  if (!local_fonts_enabled) {
    return nullptr;
  }

  if (IsValid() && IsLoading()) {
    const SimpleFontData* fallback_font_data =
        CreateLoadingFallbackFontData(font_description);
    ReportFontLookup(font_description, fallback_font_data,
                     true /* is_loading_fallback */);
    return fallback_font_data;
  }

  // FIXME(drott) crbug.com/627143: We still have the issue of matching
  // family name instead of postscript name for local fonts. However, we
  // should definitely not try to take into account the full requested
  // font description including the width, slope, weight styling when
  // trying to match against local fonts. An unstyled FontDescription
  // needs to be used here, or practically none at all. Instead we
  // should only look for the postscript or full font name.
  // However, when passing a style-neutral FontDescription we can't
  // match Roboto Bold and Thin anymore on Android given the CSS Google
  // Fonts sends, compare crbug.com/765980. So for now, we continue to
  // pass font_description to avoid breaking Google Fonts.
  FontDescription unstyled_description(font_description);
#if !BUILDFLAG(IS_ANDROID)
  unstyled_description.SetStretch(kNormalWidthValue);
  unstyled_description.SetStyle(kNormalSlopeValue);
  unstyled_description.SetWeight(kNormalWeightValue);
#endif
  // We're using the FontCache here to perform local unique lookup, including
  // potentially doing GMSCore lookups for fonts available through that, mainly
  // to retrieve and get access to the SkTypeface.
  const SimpleFontData* unique_lookup_result = FontCache::Get().GetFontData(
      unstyled_description, font_name_, AlternateFontName::kLocalUniqueFace);

  sk_sp<SkTypeface> typeface(unique_lookup_result->PlatformData().TypefaceSp());

  // From the SkTypeface, here we're reusing the FontCustomPlatformData code
  // which performs application of font-variation-settings, optical sizing and
  // mapping of style (stretch, style, weight) to canonical variation axes. (See
  // corresponding code in RemoteFontFaceSource). For the size argument,
  // specifying 0, as the font instances returned from the font cache are
  // usually memory-mapped, and not kept and decoded in memory as in
  // RemoteFontFaceSource.
  FontCustomPlatformData* custom_platform_data =
      FontCustomPlatformData::Create(typeface, 0);
  SimpleFontData* font_data_variations_palette_applied =
      MakeGarbageCollected<SimpleFontData>(
          custom_platform_data->GetFontPlatformData(
              font_description.EffectiveFontSize(),
              font_description.AdjustedSpecifiedSize(),
              font_description.IsSyntheticBold() &&
                  font_description.SyntheticBoldAllowed(),
              font_description.IsSyntheticItalic() &&
                  font_description.SyntheticItalicAllowed(),
              font_description.GetFontSelectionRequest(),
              font_selection_capabilities, font_description.FontOpticalSizing(),
              font_description.TextRendering(),
              font_description.GetFontVariantAlternates()
                  ? font_description.GetFontVariantAlternates()
                        ->GetResolvedFontFeatures()
                  : ResolvedFontFeatures(),
              font_description.Orientation(),
              font_description.VariationSettings(),
              font_description.GetFontPalette()));

  histograms_.Record(font_data_variations_palette_applied);
  ReportFontLookup(unstyled_description, font_data_variations_palette_applied);
  return font_data_variations_palette_applied;
}

void LocalFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded()) {
    return;
  }

  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get().GetFontUniqueNameLookup();
  DCHECK(unique_name_lookup);
  unique_name_lookup->PrepareFontUniqueNameLookup(
      WTF::BindOnce(&LocalFontFaceSource::NotifyFontUniqueNameLookupReady,
                    WrapWeakPersistent(this)));
  face_->DidBeginLoad();
}

void LocalFontFaceSource::NotifyFontUniqueNameLookupReady() {
  ClearTable();

  if (face_->FontLoaded(this)) {
    font_selector_->FontFaceInvalidated(
        FontInvalidationReason::kGeneralInvalidation);
  }
}

bool LocalFontFaceSource::IsLoaded() const {
  return IsLocalNonBlocking();
}

bool LocalFontFaceSource::IsLoading() const {
  return !IsLocalNonBlocking();
}

bool LocalFontFaceSource::IsValid() const {
  return IsLoading() || IsLocalFontAvailable(FontDescription());
}

void LocalFontFaceSource::LocalFontHistograms::Record(bool load_success) {
  if (reported_) {
    return;
  }
  reported_ = true;
  base::UmaHistogramBoolean("WebFont.LocalFontUsed", load_success);
}

void LocalFontFaceSource::Trace(Visitor* visitor) const {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  CSSFontFaceSource::Trace(visitor);
}

void LocalFontFaceSource::ReportFontLookup(
    const FontDescription& font_description,
    const SimpleFontData* font_data,
    bool is_loading_fallback) {
  font_selector_->ReportFontLookupByUniqueNameOnly(
      font_name_, font_description, font_data, is_loading_fallback);
}

}  // namespace blink
