/*
 * Copyright (C) 2007, 2008, 2010, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_font_face_source.h"

#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_key.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

namespace {
// An excessive amount of SimpleFontData objects is generated from
// CSSFontFaceSource if a lot of varying FontDescriptions point to a web
// font. These FontDescriptions can vary in size, font-feature-settings or
// font-variation settings. Well known cases are animations of font-variation
// settings, compare crbug.com/778352. For a start, let's reduce this number to
// 1024, which is still a large number and should have enough steps for font
// animations from the same font face source, but avoids unbounded growth.
const size_t kMaxCachedFontData = 1024;
}  // namespace

namespace blink {

CSSFontFaceSource::~CSSFontFaceSource() = default;

scoped_refptr<SimpleFontData> CSSFontFaceSource::GetFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  // If the font hasn't loaded or an error occurred, then we've got nothing.
  if (!IsValid())
    return nullptr;

  if (IsLocalNonBlocking()) {
    // We're local. Just return a SimpleFontData from the normal cache.
    return CreateFontData(font_description, font_selection_capabilities);
  }

  bool is_unique_match = false;
  FontCacheKey key =
      font_description.CacheKey(FontFaceCreationParams(), is_unique_match);

  // Get or create the font data. Take care to avoid dangling references into
  // font_data_table_, because it is modified below during pruning.
  scoped_refptr<SimpleFontData> font_data;
  {
    auto* it = font_data_table_.insert(key, nullptr).stored_value;
    if (!it->value)
      it->value = CreateFontData(font_description, font_selection_capabilities);
    font_data = it->value;
  }

  font_cache_key_age.PrependOrMoveToFirst(key);
  PruneOldestIfNeeded();

  DCHECK_LE(font_data_table_.size(), kMaxCachedFontData);
  // No release, because fontData is a reference to a RefPtr that is held in the
  // font_data_table_.
  return font_data;
}

void CSSFontFaceSource::PruneOldestIfNeeded() {
  if (font_cache_key_age.size() > kMaxCachedFontData) {
    DCHECK_EQ(font_cache_key_age.size() - 1, kMaxCachedFontData);
    FontCacheKey& key = font_cache_key_age.back();
    auto font_data_entry = font_data_table_.Take(key);
    font_cache_key_age.pop_back();
    DCHECK_EQ(font_cache_key_age.size(), kMaxCachedFontData);
    if (font_data_entry && font_data_entry->GetCustomFontData())
      font_data_entry->GetCustomFontData()->ClearFontFaceSource();
  }
}

void CSSFontFaceSource::PruneTable() {
  if (font_data_table_.IsEmpty())
    return;

  for (const auto& item : font_data_table_) {
    SimpleFontData* font_data = item.value.get();
    if (font_data && font_data->GetCustomFontData())
      font_data->GetCustomFontData()->ClearFontFaceSource();
  }
  font_cache_key_age.clear();
  font_data_table_.clear();
}

}  // namespace blink
