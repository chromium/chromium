// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/local_font_face_source.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/css_custom_font_data.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
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
      FontGlobalContext::Get()->GetFontUniqueNameLookup();
  if (!unique_name_lookup)
    return true;
  return unique_name_lookup->IsFontUniqueNameLookupReadyForSyncLookup();
}

bool LocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) const {
  return FontCache::GetFontCache()->IsPlatformFontUniqueNameMatchAvailable(
      font_description, font_name_);
}

scoped_refptr<SimpleFontData>
LocalFontFaceSource::CreateLoadingFallbackFontData(
    const FontDescription& font_description) {
  FontCachePurgePreventer font_cache_purge_preventer;
  scoped_refptr<SimpleFontData> temporary_font =
      FontCache::GetFontCache()->GetLastResortFallbackFont(font_description,
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
  if (!IsValid())
    return nullptr;

  if (IsValid() && IsLoading()) {
    return CreateLoadingFallbackFontData(font_description);
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
#if !defined(OS_ANDROID)
  unstyled_description.SetStretch(NormalWidthValue());
  unstyled_description.SetStyle(NormalSlopeValue());
  unstyled_description.SetWeight(NormalWeightValue());
#endif
  scoped_refptr<SimpleFontData> font_data =
      FontCache::GetFontCache()->GetFontData(
          unstyled_description, font_name_,
          AlternateFontName::kLocalUniqueFace);
  histograms_.Record(font_data.get());
  return font_data;
}

void LocalFontFaceSource::BeginLoadIfNeeded() {
  if (IsLoaded())
    return;

  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get()->GetFontUniqueNameLookup();
  DCHECK(unique_name_lookup);
  unique_name_lookup->PrepareFontUniqueNameLookup(
      WTF::Bind(&LocalFontFaceSource::NotifyFontUniqueNameLookupReady,
                WrapWeakPersistent(this)));
  face_->DidBeginLoad();
}

void LocalFontFaceSource::NotifyFontUniqueNameLookupReady() {
  PruneTable();

  if (face_->FontLoaded(this)) {
    font_selector_->FontFaceInvalidated();
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
  DEFINE_STATIC_LOCAL(EnumerationHistogram, local_font_used_histogram,
                      ("WebFont.LocalFontUsed", 2));
  local_font_used_histogram.Count(load_success ? 1 : 0);
}

void LocalFontFaceSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(face_);
  visitor->Trace(font_selector_);
  CSSFontFaceSource::Trace(visitor);
}

}  // namespace blink
