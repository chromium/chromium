// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <windows.h>

#include <dwrite.h>
#include <wrl/client.h>

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "ui/gfx/win/direct_write.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

namespace content {

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW* logical_font,
                                      NEWTEXTMETRICEXW* physical_font,
                                      DWORD font_type,
                                      LPARAM lparam) {
  std::set<std::wstring>* font_names =
      reinterpret_cast<std::set<std::wstring>*>(lparam);
  if (font_names) {
    const LOGFONTW& lf = logical_font->elfLogFont;
    if (lf.lfFaceName[0] && lf.lfFaceName[0] != '@') {
      std::wstring face_name(lf.lfFaceName);
      font_names->insert(face_name);
    }
  }
  return 1;
}

base::Value::List GetFontList_SlowBlocking_Legacy() {
  std::set<std::u16string> font_names;

  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;

  HDC hdc = ::GetDC(NULL);
  ::EnumFontFamiliesExW(hdc, &logfont, (FONTENUMPROCW)&EnumFontFamExProc,
                        (LPARAM)&font_names, 0);
  ::ReleaseDC(NULL, hdc);

  base::Value::List font_list;
  std::set<std::u16string>::iterator iter;
  for (iter = font_names.begin(); iter != font_names.end(); ++iter) {
    base::Value::List font_item;
    font_item.Append(*iter);
    font_item.Append(*iter);
    font_list.Append(std::move(font_item));
  }
  return font_list;
}

base::Value::List GetFontList_SlowBlocking() {
  TRACE_EVENT0("fonts", "GetFontList_SlowBlocking");

  base::Value::List font_list;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  // Fall back to GDI font list API if DirectWrite is unavailable
  if (!factory)
    return GetFontList_SlowBlocking_Legacy();

  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
  if (FAILED(factory->GetSystemFontCollection(&collection)))
    return GetFontList_SlowBlocking_Legacy();

  // Retrieve the localized font family name. If there is no localized name,
  // used the native name instead.
  std::string locale = base::i18n::GetConfiguredLocale();

  const UINT32 family_count = collection->GetFontFamilyCount();
  for (UINT32 family_index = 0; family_index < family_count; ++family_index) {
    Microsoft::WRL::ComPtr<IDWriteFontFamily> font_family;
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> family_names;
    if (FAILED(collection->GetFontFamily(family_index, &font_family)) ||
        FAILED(font_family->GetFamilyNames(&family_names))) {
      continue;
    }

    // Retrieve the native font family name. Try the "en-us" locale and if it's
    // not present, used the first available localized name.
    std::optional<std::string> native_name =
        gfx::win::RetrieveLocalizedString(family_names.Get(), "en-us");
    if (!native_name) {
      native_name = gfx::win::RetrieveLocalizedString(family_names.Get(), "");
      if (!native_name)
        continue;
    }

    std::optional<std::string> localized_name =
        gfx::win::RetrieveLocalizedString(family_names.Get(), locale);
    if (!localized_name)
      localized_name = native_name;

    base::Value::List font_item;
    font_item.Append(native_name.value());
    font_item.Append(localized_name.value());
    font_list.Append(std::move(font_item));
  }
  std::sort(font_list.begin(), font_list.end());
  return font_list;
}

}  // namespace content
