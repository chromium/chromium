// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace blink {

FontUniqueNameLookupWin::FontUniqueNameLookupWin() = default;

FontUniqueNameLookupWin::~FontUniqueNameLookupWin() = default;

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueName(
    const String& font_unique_name) {
  return MatchUniqueNameSingleLookup(font_unique_name);
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueNameSingleLookup(
    const String& font_unique_name) {
  base::File font_file;
  uint32_t ttc_index = 0;

  EnsureServiceConnected();

  bool matching_mojo_success =
      service_->MatchUniqueFont(font_unique_name, &font_file, &ttc_index);
  DCHECK(matching_mojo_success);

  return InstantiateFromFileAndTtcIndex(std::move(font_file), ttc_index);
}

// Used for font matching with single lookup case only.
sk_sp<SkTypeface> FontUniqueNameLookupWin::InstantiateFromFileAndTtcIndex(
    base::File file_handle,
    uint32_t ttc_index) {
  FILE* cfile = base::FileToFILE(std::move(file_handle), "rb");
  if (!cfile) {
    return nullptr;
  }
  auto data = SkData::MakeFromFILE(cfile);
  base::CloseFile(cfile);
  if (!data) {
    return nullptr;
  }
  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  return mgr->makeFromData(std::move(data), ttc_index);
}

bool FontUniqueNameLookupWin::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    EnsureServiceConnected();
  }

  return true;
}

void FontUniqueNameLookupWin::EnsureServiceConnected() {
  if (service_)
    return;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      service_.BindNewPipeAndPassReceiver());
}

void FontUniqueNameLookupWin::Init() {
  if (!base::FeatureList::IsEnabled(features::kPrefetchFontLookupTables))
    return;

  EnsureServiceConnected();
}

}  // namespace blink
