// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_

#include "components/services/font_data/public/mojom/font_data_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// Performs the IPC towards the browser process for font unique name
// matching. Direct individual sync Mojo IPC calls are made to lookup fonts,
// and the class reponds synchronously.
class FontUniqueNameLookupWin : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupWin();
  FontUniqueNameLookupWin(const FontUniqueNameLookupWin&) = delete;
  FontUniqueNameLookupWin& operator=(const FontUniqueNameLookupWin&) = delete;
  ~FontUniqueNameLookupWin() override;
  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

  bool IsFontUniqueNameLookupReadyForSyncLookup() override;

  void Init() override;

 private:
  void EnsureServiceConnected();
  void EnsureFontDataServiceConnected();

  sk_sp<SkTypeface> MatchUniqueNameSingleLookup(const String& font_unique_name);
  sk_sp<SkTypeface> MatchUniqueNameViaFontDataService(
      const String& font_unique_name);

  sk_sp<SkTypeface> InstantiateFromFileAndTtcIndex(base::File file_handle,
                                                   uint32_t ttc_index);
  sk_sp<SkTypeface> InstantiateFromMatchResult(
      font_data_service::mojom::blink::MatchFamilyNameResultPtr match_result);

  mojo::Remote<mojom::blink::DWriteFontProxy> service_;
  mojo::Remote<font_data_service::mojom::blink::FontDataService>
      font_data_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_
