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
  if (!unique_name_lookup)
    return true;
  return unique_name_lookup->IsFontUniqueNameLookupReadyForSyncLookup();
}

bool LocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) const {
  // TODO(crbug.com/1027158): Remove metrics code after metrics collected.
  // TODO(crbug.com/1025945): Properly handle Windows prior to 10 and Android.
  bool font_available = FontCache::Get().IsPlatformFontUniqueNameMatchAvailable(
      font_description, font_name_);
  if (font_available)
    font_selector_->ReportSuccessfulLocalFontMatch(font_name_);
  else
    font_selector_->ReportFailedLocalFontMatch(font_name_);
  return font_available;
}

scoped_refptr<SimpleFontData>
LocalFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  FontCachePurgePreventer font_cache_purge_preventer;
  scoped_refptr<SimpleFontData> temporary_font =
      FontCache::Get().GetLastResortFallbackFont(font_description,
                                                 kDoNotRetain);
  if (!temporary_font) {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CSSCustomFontData> css_font_data =
      CSSCustomFontData::Create(this, CSSCustomFontData::kVisibleFallback);
  return SimpleFontData::Create(temporary_font->PlatformData(), css_font_data);
}

scoped_refptr<SimpleFontData> LocalFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities&) {
  if (!IsValid()) {
    ReportFontLookup(font_description, nullptr);
    return nullptr;
  }

  bool local_fonts_enabled = true;
  probe::LocalFontsEnabled(font_selector_->GetExecutionContext(),
                           &local_fonts_enabled);

  if (!local_fonts_enabled)
    return nullptr;

  if (IsValid() && IsLoading()) {
    scoped_refptr<SimpleFontData> fallback_font_data =
        CreateLoadingFallbackFontData(font_description);
    ReportFontLookup(font_description, fallback_font_data.get(),
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
  unstyled_description.SetStretch(NormalWidthValue());
  unstyled_description.SetStyle(NormalSlopeValue());
  unstyled_description.SetWeight(NormalWeightValue());
#endif
  // TODO(https://crbug.com/1302264): Enable passing down of font-palette
  // information here (font_description.GetFontPalette()).
  scoped_refptr<SimpleFontData> font_data = FontCache::Get().GetFontData(
      unstyled_description, font_name_, AlternateFontName::kLocalUniqueFace);
  histograms_.Record(font_data.get());
  ReportFontLookup(unstyled_description, font_data.get());
  return font_data;
}

void LocalFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded())
    return;

  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get().GetFontUniqueNameLookup();
  DCHECK(unique_name_lookup);
  unique_name_lookup->PrepareFontUniqueNameLookup(
      WTF::BindOnce(&LocalFontFaceSource::NotifyFontUniqueNameLookupReady,
                    WrapWeakPersistent(this)));
  face_->DidBeginLoad();
}

void LocalFontFaceSource::NotifyFontUniqueNameLookupReady() {
  PruneTable();

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
  if (reported_)
    return;
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
    SimpleFontData* font_data,
    bool is_loading_fallback) {
  font_selector_->ReportFontLookupByUniqueNameOnly(
      font_name_, font_description, font_data, is_loading_fallback);
}

}  // namespace blink
