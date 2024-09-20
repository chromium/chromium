// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"

#include "base/files/file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {
namespace {

void LogFontLatencyFailure(base::TimeDelta delta) {
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Android.FontLookup.Blink.DLFontsLatencyFailure2", delta,
      base::Microseconds(1), base::Seconds(10), 50);
}

void LogFontLatencySuccess(base::TimeDelta delta) {
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Android.FontLookup.Blink.DLFontsLatencySuccess2", delta,
      base::Microseconds(1), base::Seconds(10), 50);
}
}  // namespace

FontUniqueNameLookupAndroid::~FontUniqueNameLookupAndroid() = default;

void FontUniqueNameLookupAndroid::PrepareFontUniqueNameLookup(
    NotifyFontUniqueNameLookupReady callback) {
  DCHECK(!font_table_matcher_.get());
  DCHECK(RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled());

  pending_callbacks_.push_back(std::move(callback));

  // We bind the service on the first call to PrepareFontUniqueNameLookup. After
  // that we do not need to make additional IPC requests to retrieve the table.
  // The observing callback was added to the list, so all clients will be
  // informed when the lookup table has arrived.
  if (pending_callbacks_.size() > 1)
    return;

  EnsureServiceConnected();

  firmware_font_lookup_service_->GetUniqueNameLookupTable(WTF::BindOnce(
      &FontUniqueNameLookupAndroid::ReceiveReadOnlySharedMemoryRegion,
      WTF::Unretained(this)));
}

bool FontUniqueNameLookupAndroid::IsFontUniqueNameLookupReadyForSyncLookup() {
  if (!RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled())
    return true;

  EnsureServiceConnected();

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
  firmware_font_lookup_service_->GetUniqueNameLookupTableIfAvailable(
      &sync_available_from_mojo, &shared_memory_region);
  sync_available_ = sync_available_from_mojo;

  if (*sync_available_) {
    // Adopt the shared memory region, do not notify anyone in callbacks as
    // PrepareFontUniqueNameLookup must not have been called yet. Just return
    // true from this function.
    // TODO(crbug.com/1416529): Investigate why pending_callbacks is not 0 in
    // some cases when kPrefetchFontLookupTables is enabled
    if (pending_callbacks_.size() != 0) {
      LOG(WARNING) << "Number of pending callbacks not zero";
    }
    ReceiveReadOnlySharedMemoryRegion(std::move(shared_memory_region));
  }

  // If it wasn't available synchronously LocalFontFaceSource has to call
  // PrepareFontUniqueNameLookup.
  return *sync_available_;
}

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueName(
    const String& font_unique_name) {
  if (!IsFontUniqueNameLookupReadyForSyncLookup())
    return nullptr;
  sk_sp<SkTypeface> result_font =
      MatchUniqueNameFromFirmwareFonts(font_unique_name);
  if (result_font)
    return result_font;
  if (RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled()) {
    return MatchUniqueNameFromDownloadableFonts(font_unique_name);
  } else {
    return nullptr;
  }
}

void FontUniqueNameLookupAndroid::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled()) {
    EnsureServiceConnected();
    if (android_font_lookup_service_) {
      // WTF::Unretained is safe here because |this| owns
      // |android_font_lookup_service_|.
      android_font_lookup_service_->FetchAllFontFiles(
          WTF::BindOnce(&FontUniqueNameLookupAndroid::FontsPrefetched,
                        WTF::Unretained(this)));
    }
  }
  if (base::FeatureList::IsEnabled(features::kPrefetchFontLookupTables) &&
      RuntimeEnabledFeatures::FontSrcLocalMatchingEnabled()) {
    // This call primes IsFontUniqueNameLookupReadyForSyncLookup() by
    // asynchronously fetching the font table so it will be ready when needed.
    // It isn't needed now, so base::DoNothing() is passed as the callback.
    PrepareFontUniqueNameLookup(base::DoNothing());
  }
}

void FontUniqueNameLookupAndroid::EnsureServiceConnected() {
  if (firmware_font_lookup_service_ &&
      (!RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled() ||
       android_font_lookup_service_))
    return;

  if (!firmware_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        firmware_font_lookup_service_.BindNewPipeAndPassReceiver());
  }

  if (RuntimeEnabledFeatures::AndroidDownloadableFontsMatchingEnabled() &&
      !android_font_lookup_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        android_font_lookup_service_.BindNewPipeAndPassReceiver());
  }
}

void FontUniqueNameLookupAndroid::ReceiveReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion shared_memory_region) {
  font_table_matcher_ =
      std::make_unique<FontTableMatcher>(shared_memory_region.Map());
  while (!pending_callbacks_.empty()) {
    NotifyFontUniqueNameLookupReady callback = pending_callbacks_.TakeFirst();
    std::move(callback).Run();
  }
}

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueNameFromFirmwareFonts(
    const String& font_unique_name) {
  std::optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().c_str());
  if (!match_result) {
    return nullptr;
  }
  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  return mgr->makeFromFile(match_result->font_path.c_str(),
                           match_result->ttc_index);
}

bool FontUniqueNameLookupAndroid::RequestedNameInQueryableFonts(
    const String& font_unique_name) {
  if (!queryable_fonts_) {
    SCOPED_UMA_HISTOGRAM_TIMER("Android.FontLookup.Blink.GetTableLatency");
    Vector<String> retrieved_fonts;
    android_font_lookup_service_->GetUniqueNameLookupTable(&retrieved_fonts);
    queryable_fonts_ = std::move(retrieved_fonts);
  }
  return queryable_fonts_ && queryable_fonts_->Contains(String::FromUTF8(
                                 IcuFoldCase(font_unique_name.Utf8())));
}

sk_sp<SkTypeface>
FontUniqueNameLookupAndroid::MatchUniqueNameFromDownloadableFonts(
    const String& font_unique_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!android_font_lookup_service_.is_bound()) {
    LOG(ERROR) << "Service not connected.";
    return nullptr;
  }

  if (!RequestedNameInQueryableFonts(font_unique_name))
    return nullptr;

  base::File font_file;
  String case_folded_unique_font_name =
      String::FromUTF8(IcuFoldCase(font_unique_name.Utf8()));

  base::ElapsedTimer elapsed_timer;

  auto it = prefetched_font_map_.find(case_folded_unique_font_name);
  if (it != prefetched_font_map_.end()) {
    font_file = it->value.Duplicate();
  } else if (!android_font_lookup_service_->MatchLocalFontByUniqueName(
                 case_folded_unique_font_name, &font_file)) {
    LOG(ERROR)
        << "Mojo method returned false for case-folded unique font name: "
        << case_folded_unique_font_name;
    LogFontLatencyFailure(elapsed_timer.Elapsed());
    return nullptr;
  }

  if (!font_file.IsValid()) {
    LOG(ERROR) << "Received platform font handle invalid, fd: "
               << font_file.GetPlatformFile();
    LogFontLatencyFailure(elapsed_timer.Elapsed());
    return nullptr;
  }

  sk_sp<SkData> font_data = SkData::MakeFromFD(font_file.GetPlatformFile());

  if (!font_data || font_data->isEmpty()) {
    LOG(ERROR) << "Received file descriptor has 0 size.";
    LogFontLatencyFailure(elapsed_timer.Elapsed());
    return nullptr;
  }

  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> return_typeface = mgr->makeFromData(font_data);

  if (!return_typeface) {
    LogFontLatencyFailure(elapsed_timer.Elapsed());
    LOG(ERROR) << "Cannot instantiate SkTypeface from font blob SkData.";
  }

  LogFontLatencySuccess(elapsed_timer.Elapsed());
  return return_typeface;
}

void FontUniqueNameLookupAndroid::FontsPrefetched(
    HashMap<String, base::File> font_files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  prefetched_font_map_ = std::move(font_files);

  if (base::FeatureList::IsEnabled(features::kPrefetchFontLookupTables)) {
    // The |prefetched_font_map_| contains all the fonts that are available from
    // the AndroidFontLookup service. We can directly set |queryable_fonts_|
    // here from the map keys since |queryable_fonts_| is used to check which
    // fonts can be fetched from the AndroidFontLookup service.
    queryable_fonts_ = Vector<String>();
    CopyKeysToVector(prefetched_font_map_, *queryable_fonts_);
  }
}

}  // namespace blink
