// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/font_fallback_win.h"

#include <math.h>

#include <algorithm>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

const size_t kMaxFamilyCacheSize = 10;

std::wstring MakeCacheKey(const wchar_t* base_family_name,
                          const wchar_t* locale) {
  std::wstring cache_key(base_family_name);
  return cache_key + L"_" + locale;
}

}  // namespace

HRESULT FontFallback::Create(FontFallback** font_fallback_out,
                             DWriteFontCollectionProxy* collection) {
  return Microsoft::WRL::MakeAndInitialize<FontFallback>(font_fallback_out,
                                                         collection);
}

FontFallback::FontFallback() = default;
FontFallback::~FontFallback() = default;

HRESULT FontFallback::MapCharacters(IDWriteTextAnalysisSource* source,
                                    UINT32 text_position,
                                    UINT32 text_length,
                                    IDWriteFontCollection* base_font_collection,
                                    const wchar_t* base_family_name,
                                    DWRITE_FONT_WEIGHT base_weight,
                                    DWRITE_FONT_STYLE base_style,
                                    DWRITE_FONT_STRETCH base_stretch,
                                    UINT32* mapped_length,
                                    IDWriteFont** mapped_font,
                                    FLOAT* scale) {
  *mapped_font = nullptr;
  *mapped_length = 1;
  *scale = 1.0;

  const WCHAR* text = nullptr;
  UINT32 chunk_length = 0;
  if (FAILED(source->GetTextAtPosition(text_position, &text, &chunk_length))) {
    DCHECK(false);
    return E_FAIL;
  }
  std::u16string text_chunk;
  base::WideToUTF16(text, std::min(chunk_length, text_length), &text_chunk);

  if (text_chunk.size() == 0) {
    DCHECK(false);
    return E_INVALIDARG;
  }

  base_family_name = base_family_name ? base_family_name : L"";

  const WCHAR* locale = nullptr;
  // |locale_text_length| is actually the length of text with the locale, not
  // the length of the locale string itself.
  UINT32 locale_text_length = 0;
  source->GetLocaleName(text_position /*textPosition*/, &locale_text_length,
                        &locale);

  locale = locale ? locale : L"";

  size_t mapped_length_size_t = *mapped_length;
  if (GetCachedFont(text_chunk, base_family_name, locale, base_weight,
                    base_style, base_stretch, mapped_font,
                    &mapped_length_size_t)) {
    DCHECK(*mapped_font);
    DCHECK_GT(mapped_length_size_t, 0u);
    *mapped_length = base::checked_cast<UINT32>(mapped_length_size_t);
    return S_OK;
  }

  TRACE_EVENT0("dwrite,fonts", "FontFallback::MapCharacters (IPC)");

  blink::mojom::MapCharactersResultPtr result;

  if (!GetFontProxy().MapCharacters(
          text_chunk,
          blink::mojom::DWriteFontStyle::New(base_weight, base_style,
                                             base_stretch),
          base::WideToUTF16(locale), source->GetParagraphReadingDirection(),
          base::WideToUTF16(base_family_name), &result)) {
    DCHECK(false);
    return E_FAIL;
  }

  // We don't cache scale in the fallback cache, and Skia ignores scale anyway.
  // If we ever get a result that's significantly different from 1 we may need
  // to consider whether it's worth doing the work to plumb it through.
  DCHECK(fabs(*scale - 1.0f) < 0.00001);

  *mapped_length = result->mapped_length;
  *scale = result->scale;

  if (result->family_index == UINT32_MAX) {
    return S_OK;
  }

  mswr::ComPtr<IDWriteFontFamily> family;
  // It would be nice to find a way to determine at runtime if |collection_| is
  // a proxy collection, or just a generic IDWriteFontCollection. Unfortunately
  // I can't find a way to get QueryInterface to return the actual class when
  // using mswr::RuntimeClass. If we could use QI, we can fallback on
  // FindFontFamily if the proxy is not available.
  if (!collection_->GetFontFamily(result->family_index, result->family_name,
                                  &family)) {
    DCHECK(false);
    return E_FAIL;
  }

  if (FAILED(family->GetFirstMatchingFont(
          static_cast<DWRITE_FONT_WEIGHT>(result->font_style->font_weight),
          static_cast<DWRITE_FONT_STRETCH>(result->font_style->font_stretch),
          static_cast<DWRITE_FONT_STYLE>(result->font_style->font_slant),
          mapped_font))) {
    DCHECK(false);
    return E_FAIL;
  }

  DCHECK(*mapped_font);
  AddCachedFamily(std::move(family), base_family_name, locale);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
FontFallback::RuntimeClassInitialize(DWriteFontCollectionProxy* collection) {
  collection_ = collection;
  return S_OK;
}

bool FontFallback::GetCachedFont(const std::u16string& text,
                                 const wchar_t* base_family_name,
                                 const wchar_t* locale,
                                 DWRITE_FONT_WEIGHT base_weight,
                                 DWRITE_FONT_STYLE base_style,
                                 DWRITE_FONT_STRETCH base_stretch,
                                 IDWriteFont** font,
                                 size_t* mapped_length) {
  base::AutoLock guard(lock_);
  std::map<std::wstring, std::list<mswr::ComPtr<IDWriteFontFamily>>>::iterator
      it = fallback_family_cache_.find(MakeCacheKey(base_family_name, locale));
  if (it == fallback_family_cache_.end())
    return false;

  TRACE_EVENT0("dwrite,fonts", "FontFallback::GetCachedFont");

  std::list<mswr::ComPtr<IDWriteFontFamily>>& family_list = it->second;
  std::list<mswr::ComPtr<IDWriteFontFamily>>::iterator family_iterator;
  for (family_iterator = family_list.begin();
       family_iterator != family_list.end(); ++family_iterator) {
    mswr::ComPtr<IDWriteFont> matched_font;

    if (FAILED((*family_iterator)
                   ->GetFirstMatchingFont(base_weight, base_stretch, base_style,
                                          &matched_font))) {
      continue;
    }

    // |character_index| tracks how much of the string we have read. This is
    // different from |mapped_length| because ReadUnicodeCharacter can advance
    // |character_index| even if the character cannot be mapped (invalid
    // surrogate pair or font does not contain a matching glyph).
    size_t character_index = 0;
    size_t length = 0;  // How much of the text can actually be mapped.
    while (character_index < text.length()) {
      BOOL exists = false;
      base_icu::UChar32 character = 0;
      if (!base::ReadUnicodeCharacter(text.c_str(), text.length(),
                                      &character_index, &character))
        break;
      if (FAILED(matched_font->HasCharacter(character, &exists)) || !exists)
        break;
      character_index++;
      length = character_index;
    }

    if (length > 0) {
      // Move the current family to the front of the list
      family_list.splice(family_list.begin(), family_list, family_iterator);

      matched_font.CopyTo(font);
      *mapped_length = length;
      return true;
    }
  }

  return false;
}

void FontFallback::AddCachedFamily(
    Microsoft::WRL::ComPtr<IDWriteFontFamily> family,
    const wchar_t* base_family_name,
    const wchar_t* locale) {
  base::AutoLock guard(lock_);
  // Note: If the requested locale does not disambiguate Han ideographs, caching
  // by locale may prime the cache with one CJK font for the first request,
  // which may be unsuitable for the next request. For example: While specifying
  // an ambiguous locale, requesting certain Chinese characters first, DWrite
  // will give us a simplified Chinese font, then requesting a Korean character
  // later may return a Chinese font for the Korean character. This is prevented
  // on the Blink side by passing a disambiguating locale.
  std::list<mswr::ComPtr<IDWriteFontFamily>>& family_list =
      fallback_family_cache_[MakeCacheKey(base_family_name, locale)];
  family_list.push_front(std::move(family));

  while (family_list.size() > kMaxFamilyCacheSize)
    family_list.pop_back();
}

blink::mojom::DWriteFontProxy& FontFallback::GetFontProxy() {
  return collection_->GetFontProxy();
}

}  // namespace content
