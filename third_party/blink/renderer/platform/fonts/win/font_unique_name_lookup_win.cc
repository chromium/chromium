// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace blink {

FontUniqueNameLookupWin::FontUniqueNameLookupWin() = default;

FontUniqueNameLookupWin::~FontUniqueNameLookupWin() = default;

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueName(
    const String& font_unique_name) {
  if (RuntimeEnabledFeatures::FontDataServiceEnabled()) {
    return MatchUniqueNameViaFontDataService(font_unique_name);
  }
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
  if (RuntimeEnabledFeatures::FontDataServiceEnabled()) {
    EnsureFontDataServiceConnected();
  } else {
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

void FontUniqueNameLookupWin::EnsureFontDataServiceConnected() {
  if (font_data_service_) {
    return;
  }
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      font_data_service_.BindNewPipeAndPassReceiver());
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueNameViaFontDataService(
    const String& font_unique_name) {
  EnsureFontDataServiceConnected();

  font_data_service::mojom::blink::MatchFamilyNameResultPtr match_result;
  bool success =
      font_data_service_->MatchLocalFont(font_unique_name, &match_result);

  if (!success || !match_result) {
    return nullptr;
  }

  return InstantiateFromMatchResult(std::move(match_result));
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::InstantiateFromMatchResult(
    font_data_service::mojom::blink::MatchFamilyNameResultPtr match_result) {
  if (!match_result->typeface_data) {
    return nullptr;
  }

  SkFontArguments args;
  args.setCollectionIndex(match_result->ttc_index);

  sk_sp<SkData> data;

  if (match_result->typeface_data->is_font_file()) {
    TRACE_EVENT("fonts", "FontUniqueNameLookupWin - using mapped file");
    // Memory-map the font file for zero-copy access. Ownership is
    // transferred to SkData via the release proc.
    auto mapped_file = std::make_unique<base::MemoryMappedFile>();
    if (mapped_file->Initialize(std::move(
            match_result->typeface_data->get_font_file()->file_handle))) {
      data = SkData::MakeWithProc(
          mapped_file->data(), mapped_file->length(),
          [](const void*, void* ctx) {
            delete static_cast<base::MemoryMappedFile*>(ctx);
          },
          mapped_file.get());
      if (data) {
        mapped_file.release();
      }
    }
  } else if (match_result->typeface_data->is_region() &&
             match_result->typeface_data->get_region().IsValid()) {
    TRACE_EVENT("fonts", "FontUniqueNameLookupWin - using shared memory");
    auto mapping = std::make_unique<base::ReadOnlySharedMemoryMapping>(
        match_result->typeface_data->get_region().Map());
    if (mapping->IsValid()) {
      data = SkData::MakeWithProc(
          mapping->memory(), mapping->size(),
          [](const void*, void* ctx) {
            delete static_cast<base::ReadOnlySharedMemoryMapping*>(ctx);
          },
          mapping.get());
      if (data) {
        mapping.release();
      }
    }
  }

  if (!data) {
    return nullptr;
  }

  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  return mgr->makeFromStream(std::make_unique<SkMemoryStream>(std::move(data)),
                             args);
}

void FontUniqueNameLookupWin::Init() {
  if (RuntimeEnabledFeatures::FontDataServiceEnabled()) {
    EnsureFontDataServiceConnected();
    return;
  }
  if (!base::FeatureList::IsEnabled(features::kPrefetchFontLookupTables))
    return;

  EnsureServiceConnected();
}

}  // namespace blink
