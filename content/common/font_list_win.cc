// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <dwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/i18n/rtl.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

std::unique_ptr<base::ListValue> GetFontList_SlowBlocking() {
  TRACE_EVENT0("fonts", "GetFontList_SlowBlocking");

  std::unique_ptr<base::ListValue> font_list(new base::ListValue);

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (!factory)
    return font_list;

  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;
  if (FAILED(factory->GetSystemFontCollection(&collection)))
    return font_list;

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
    base::Optional<std::string> native_name =
        gfx::win::RetrieveLocalizedString(family_names.Get(), "en-us");
    if (!native_name) {
      native_name = gfx::win::RetrieveLocalizedString(family_names.Get(), "");
      if (!native_name)
        continue;
    }

    base::Optional<std::string> localized_name =
        gfx::win::RetrieveLocalizedString(family_names.Get(), locale);
    if (!localized_name)
      localized_name = native_name;

    auto font_item = std::make_unique<base::ListValue>();
    font_item->AppendString(native_name.value());
    font_item->AppendString(localized_name.value());
    font_list->Append(std::move(font_item));
  }

  return font_list;
}

}  // namespace content
