// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

// Performs the IPC towards the browser process for font unique name
// matching. This class operates in one of two lookup modes, depending on
// lookup_mode_. On Windows 10 or when IDWriteFontFactory3 is available, direct
// individual sync Mojo IPC calls are made too lookup fonts - and the class
// reponds synchronously.  On Windows 7 & 8, a shared memory region is retrieved
// asynchronously, then lookups are performed against that table. When the
// asynchronous request to retrieve the table completes, the clients are
// notified. And once the table was retrieved, this class returns to operating
// in synchronous mode as matching can be performed instantly.
class FontUniqueNameLookupWin : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupWin();
  ~FontUniqueNameLookupWin() override;
  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

  bool IsFontUniqueNameLookupReadyForSyncLookup() override;

  void PrepareFontUniqueNameLookup(
      NotifyFontUniqueNameLookupReady callback) override;

 private:
  void EnsureServiceConnected();

  sk_sp<SkTypeface> MatchUniqueNameLookupTable(const String& font_unique_name);
  sk_sp<SkTypeface> MatchUniqueNameSingleLookup(const String& font_unique_name);

  sk_sp<SkTypeface> InstantiateFromPathAndTtcIndex(
      base::FilePath font_file_path,
      uint32_t ttc_index);

  mojo::Remote<mojom::blink::DWriteFontProxy> service_;
  WTF::Deque<NotifyFontUniqueNameLookupReady> pending_callbacks_;
  base::Optional<blink::mojom::UniqueFontLookupMode> lookup_mode_;
  base::Optional<bool> sync_available_;
  void ReceiveReadOnlySharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion shared_memory_region);

  DISALLOW_COPY_AND_ASSIGN(FontUniqueNameLookupWin);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_
