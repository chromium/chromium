// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "mojo/public/mojom/base/shared_memory.mojom-blink.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace {

// These enum values correspond to the
// "Blink.Fonts.WindowsUniqueLocalFontInstantiationResult" histogram, new values
// can be added, but old values should never be reused.
enum WindowsUniqueLocalFontInstantiationResult {
  kSuccess = 0,
  kErrorOutsideWindowsFontsDirectory = 1,
  kErrorOther = 2,
  kMaxWindowsUniqueLocalFontInstantiationResult = 3
};

}  // namespace

namespace blink {

FontUniqueNameLookupWin::FontUniqueNameLookupWin() = default;

FontUniqueNameLookupWin::~FontUniqueNameLookupWin() = default;

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueName(
    const String& font_unique_name) {
  if (lookup_mode_ == blink::mojom::UniqueFontLookupMode::kSingleLookups)
    return MatchUniqueNameSingleLookup(font_unique_name);
  return MatchUniqueNameLookupTable(font_unique_name);
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueNameSingleLookup(
    const String& font_unique_name) {
  DCHECK(lookup_mode_ == blink::mojom::UniqueFontLookupMode::kSingleLookups);
  base::FilePath font_file_path;
  uint32_t ttc_index = 0;

  EnsureServiceConnected();

  bool matching_mojo_success =
      service_->MatchUniqueFont(font_unique_name, &font_file_path, &ttc_index);
  DCHECK(matching_mojo_success);

  if (!font_file_path.value().size())
    return nullptr;

  return InstantiateFromPathAndTtcIndex(font_file_path, ttc_index);
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueNameLookupTable(
    const String& font_unique_name) {
  DCHECK(lookup_mode_ == blink::mojom::UniqueFontLookupMode::kRetrieveTable);

  if (!IsFontUniqueNameLookupReadyForSyncLookup())
    return nullptr;

  base::Optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8());
  if (!match_result)
    return nullptr;

  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(match_result->font_path.c_str());
  return InstantiateFromPathAndTtcIndex(file_path, match_result->ttc_index);
}

sk_sp<SkTypeface> FontUniqueNameLookupWin::InstantiateFromPathAndTtcIndex(
    base::FilePath font_file_path,
    uint32_t ttc_index) {
  // Record here when a locally uniquely matched font could not be
  // instantiated. One reason could be that the font was outside the
  // C:\Windows\Fonts directory and thus not accessible due to sandbox
  // restrictions.
  sk_sp<SkTypeface> local_typeface = SkTypeface::MakeFromFile(
      font_file_path.AsUTF8Unsafe().c_str(), ttc_index);

  WindowsUniqueLocalFontInstantiationResult result = kSuccess;

  // There is a chance that some systems have managed to register fonts into the
  // Windows system font collection outside the C:\Windows\Fonts directory. For
  // sandboxing reasons, we are unable to access them here. This histogram
  // serves to quantify how often this case occurs and whether we need and
  // additional sandbox helper to open the file handle on the browser process
  // side.
  if (!local_typeface) {
    base::FilePath windows_fonts_path(L"C:\\WINDOWS\\FONTS");
    if (!windows_fonts_path.IsParent(font_file_path))
      result = kErrorOutsideWindowsFontsDirectory;
    else
      result = kErrorOther;
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, windows_unique_local_font_instantiation_histogram,
      ("Blink.Fonts.WindowsUniqueLocalFontInstantiationResult",
       kMaxWindowsUniqueLocalFontInstantiationResult));
  windows_unique_local_font_instantiation_histogram.Count(result);

  return local_typeface;
}

bool FontUniqueNameLookupWin::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (!RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled())
    return true;

  EnsureServiceConnected();

  if (!lookup_mode_.has_value()) {
    blink::mojom::UniqueFontLookupMode lookup_mode_from_mojo;
    service_->GetUniqueFontLookupMode(&lookup_mode_from_mojo);
    lookup_mode_ = lookup_mode_from_mojo;
  }

  if (lookup_mode_ == blink::mojom::UniqueFontLookupMode::kSingleLookups) {
    return true;
  }

  DCHECK(lookup_mode_ == blink::mojom::UniqueFontLookupMode::kRetrieveTable);

  // If we have the table already, we're ready for sync lookups.
  if (font_table_matcher_.get())
    return true;

  // We have previously determined via IPC whether the table is sync available.
  // Return what we found out before.
  if (sync_available_.has_value())
    return sync_available_.value();

  // If we haven't asked the browser before, probe synchronously - if the table
  // is available on the browser side, we can continue with sync operation.

  bool sync_available_from_mojo = false;
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  service_->GetUniqueNameLookupTableIfAvailable(&sync_available_from_mojo,
                                                &shared_memory_region);
  sync_available_ = sync_available_from_mojo;

  if (*sync_available_) {
    // Adopt the shared memory region, do not notify anyone in callbacks as
    // PrepareFontUniqueNameLookup must not have been called yet. Just return
    // true from this function.
    DCHECK_EQ(pending_callbacks_.size(), 0u);
    ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
  }

  // If it wasn't available synchronously LocalFontFaceSource has to call
  // PrepareFontUniqueNameLookup.
  return *sync_available_;
}

void FontUniqueNameLookupWin::EnsureServiceConnected() {
  if (service_)
    return;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      service_.BindNewPipeAndPassReceiver());
}

void FontUniqueNameLookupWin::PrepareFontUniqueNameLookup(
    NotifyFontUniqueNameLookupReady callback) {
  DCHECK(!font_table_matcher_.get());
  DCHECK(RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled());
  DCHECK(lookup_mode_ == blink::mojom::UniqueFontLookupMode::kRetrieveTable);

  pending_callbacks_.push_back(std::move(callback));

  // We bind the service on the first call to PrepareFontUniqueNameLookup. After
  // that we do not need to make additional IPC requests to retrieve the table.
  // The observing callback was added to the list, so all clients will be
  // informed when the lookup table has arrived.
  if (pending_callbacks_.size() > 1)
    return;

  EnsureServiceConnected();

  service_->GetUniqueNameLookupTable(base::BindOnce(
      &FontUniqueNameLookupWin::ReceiveReadOnlySharedMemoryRegion,
      base::Unretained(this)));
}

void FontUniqueNameLookupWin::ReceiveReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion shared_memory_region) {
  DCHECK(lookup_mode_ == blink::mojom::UniqueFontLookupMode::kRetrieveTable);
  font_table_matcher_ =
      std::make_unique<FontTableMatcher>(shared_memory_region.Map());
  while (!pending_callbacks_.IsEmpty()) {
    NotifyFontUniqueNameLookupReady callback = pending_callbacks_.TakeFirst();
    std::move(callback).Run();
  }
}

}  // namespace blink
