// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_localized_strings_win.h"

#include <stddef.h>


namespace content {

DWriteLocalizedStrings::DWriteLocalizedStrings() = default;

DWriteLocalizedStrings::~DWriteLocalizedStrings() = default;

HRESULT DWriteLocalizedStrings::FindLocaleName(const WCHAR* locale_name,
                                               UINT32* index,
                                               BOOL* exists) {
  for (size_t n = 0; n < strings_.size(); ++n) {
    if (_wcsicmp(strings_[n].first.data(), locale_name) == 0) {
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
  // wstring::size does not count the null terminator as part of the string,
  // but GetLocaleName requires the caller to reserve space for the null
  // terminator, so we need to ensure |size| is greater than the count of
  // characters.
  if (size <= strings_[index].first.size())
    return E_INVALIDARG;
  wcsncpy(locale_name, strings_[index].first.c_str(), size);
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
  // wstring::size does not count the null terminator as part of the string,
  // but GetString requires the caller to reserve space for the null terminator,
  // so we need to ensure |size| is greater than the count of characters.
  if (size <= strings_[index].second.size())
    return E_INVALIDARG;
  wcsncpy(string_buffer, strings_[index].second.c_str(), size);
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
    std::vector<std::pair<std::wstring, std::wstring>>* strings) {
  strings_.swap(*strings);
  return S_OK;
}

}  // namespace content
