// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_LOCALIZED_STRINGS_WIN_H_
#define CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_LOCALIZED_STRINGS_WIN_H_

#include <dwrite.h>
#include <wrl.h>
#include <string>
#include <utility>
#include <vector>

namespace content {

// Impements IDWriteLocalizedStrings, backed by a vector of string pairs.
class DWriteLocalizedStrings
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteLocalizedStrings> {
 public:
  DWriteLocalizedStrings();

  DWriteLocalizedStrings& operator=(const DWriteLocalizedStrings&) = delete;

  ~DWriteLocalizedStrings() override;

  // IDWriteLocalizedStrings:
  HRESULT STDMETHODCALLTYPE FindLocaleName(const WCHAR* locale_name,
                                           UINT32* index,
                                           BOOL* exists) override;
  UINT32 STDMETHODCALLTYPE GetCount() override;
  HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 index,
                                          WCHAR* locale_name,
                                          UINT32 size) override;
  HRESULT STDMETHODCALLTYPE GetLocaleNameLength(UINT32 index,
                                                UINT32* length) override;
  HRESULT STDMETHODCALLTYPE GetString(UINT32 index,
                                      WCHAR* string_buffer,
                                      UINT32 size) override;
  HRESULT STDMETHODCALLTYPE GetStringLength(UINT32 index,
                                            UINT32* length) override;

  HRESULT STDMETHODCALLTYPE RuntimeClassInitialize(
      std::vector<std::pair<std::u16string, std::u16string>>* strings);

 private:
  // List of strings. First element of each pair is the locale, and the second
  // element is the associated value. Use a vector because the expected number
  // of pairs is small (typically 1-2, rarely up to a few dozen?) and we need
  // index-based access.
  std::vector<std::pair<std::u16string, std::u16string>> strings_;
};

}  // namespace content
#endif  // CONTENT_CHILD_DWRITE_FONT_PROXY_DWRITE_LOCALIZED_STRINGS_WIN_H_
