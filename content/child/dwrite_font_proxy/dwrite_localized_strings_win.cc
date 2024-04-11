// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_localized_strings_win.h"

#include <stddef.h>

#include <string_view>

namespace content {

DWriteLocalizedStrings::DWriteLocalizedStrings() = default;

DWriteLocalizedStrings::~DWriteLocalizedStrings() = default;

HRESULT DWriteLocalizedStrings::FindLocaleName(const WCHAR* locale_name,
                                               UINT32* index,
                                               BOOL* exists) {
  static_assert(sizeof(WCHAR) == sizeof(char16_t), "WCHAR should be UTF-16.");
  const std::u16string_view locale_name_str(
      reinterpret_cast<const char16_t*>(locale_name));
  for (size_t n = 0; n < strings_.size(); ++n) {
    if (strings_[n].first == locale_name_str) {
      *index = n;
      *exists = TRUE;
      return S_OK;
    }
  }

  *index = UINT_MAX;
  *exists = FALSE;
  return S_OK;
}

UINT32 DWriteLocalizedStrings::GetCount() {
  return strings_.size();
}

HRESULT DWriteLocalizedStrings::GetLocaleName(UINT32 index,
                                              WCHAR* locale_name,
                                              UINT32 size) {
  if (index >= strings_.size())
    return E_INVALIDARG;
  // u16string::size does not count the null terminator as part of the string,
  // but GetLocaleName requires the caller to reserve space for the null
  // terminator, so we need to ensure |size| is greater than the count of
  // characters.
  if (size <= strings_[index].first.size())
    return E_INVALIDARG;
  static_assert(sizeof(WCHAR) == sizeof(char16_t), "WCHAR should be UTF-16.");
  strings_[index].first.copy(reinterpret_cast<char16_t*>(locale_name), size);
  return S_OK;
}

HRESULT DWriteLocalizedStrings::GetLocaleNameLength(UINT32 index,
                                                    UINT32* length) {
  if (index >= strings_.size())
    return E_INVALIDARG;
  // Oddly, GetLocaleNameLength requires the length to not count the null
  // terminator, even though GetLocaleName requires the output to be null
  // terminated.
  *length = strings_[index].first.size();
  return S_OK;
}

HRESULT DWriteLocalizedStrings::GetString(UINT32 index,
                                          WCHAR* string_buffer,
                                          UINT32 size) {
  if (index >= strings_.size())
    return E_INVALIDARG;
  // u16string::size does not count the null terminator as part of the string,
  // but GetString requires the caller to reserve space for the null terminator,
  // so we need to ensure |size| is greater than the count of characters.
  if (size <= strings_[index].second.size())
    return E_INVALIDARG;
  static_assert(sizeof(WCHAR) == sizeof(char16_t), "WCHAR should be UTF-16.");
  strings_[index].second.copy(reinterpret_cast<char16_t*>(string_buffer), size);
  return S_OK;
}

HRESULT DWriteLocalizedStrings::GetStringLength(UINT32 index, UINT32* length) {
  if (index >= strings_.size())
    return E_INVALIDARG;
  // Oddly, GetStringLength requires the length to not count the null
  // terminator, even though GetString requires the output to be null
  // terminated.
  *length = strings_[index].second.size();
  return S_OK;
}

HRESULT DWriteLocalizedStrings::RuntimeClassInitialize(
    std::vector<std::pair<std::u16string, std::u16string>>* strings) {
  strings_.swap(*strings);
  return S_OK;
}

}  // namespace content
