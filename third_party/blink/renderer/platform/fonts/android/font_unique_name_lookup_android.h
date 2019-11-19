// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ANDROID_FONT_UNIQUE_NAME_LOOKUP_ANDROID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ANDROID_FONT_UNIQUE_NAME_LOOKUP_ANDROID_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

#include <memory>

namespace blink {

class FontUniqueNameLookupAndroid : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupAndroid() = default;
  ~FontUniqueNameLookupAndroid() override;

  bool IsFontUniqueNameLookupReadyForSyncLookup() override;

  void PrepareFontUniqueNameLookup(
      NotifyFontUniqueNameLookupReady callback) override;

  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

 private:
  void EnsureServiceConnected();

  void ReceiveReadOnlySharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion shared_memory_region);

  mojo::Remote<mojom::blink::FontUniqueNameLookup> service_;
  WTF::Deque<NotifyFontUniqueNameLookupReady> pending_callbacks_;
  base::Optional<bool> sync_available_;

  DISALLOW_COPY_AND_ASSIGN(FontUniqueNameLookupAndroid);
};

}  // namespace blink

#endif
