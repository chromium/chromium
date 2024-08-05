// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_util.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr int kBytesInAMegabyte = 1 << 20;

}  // namespace

namespace quick_delete_util {

NSString* GetCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result& result,
    browsing_data::TimePeriod time_range) {
  if (!result.Finished()) {
    // The counter is still counting.
    return l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  std::string_view prefName = result.source()->GetPrefName();
  if (prefName == browsing_data::prefs::kDeleteCache) {
    browsing_data::BrowsingDataCounter::ResultInt cacheSizeBytes =
        static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
            &result)
            ->Value();

    // Three cases: Nonzero result for the entire cache, nonzero result for
    // a subset of cache (i.e. a finite time interval), and almost zero (less
    // than 1 MB). There is no exact information that the cache is empty so that
    // falls into the almost zero case, which is displayed as less than 1 MB.
    // Because of this, the lowest unit that can be used is MB.
    if (cacheSizeBytes < kBytesInAMegabyte) {
      return l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
    }

    NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
    formatter.allowedUnits = NSByteCountFormatterUseAll &
                             (~NSByteCountFormatterUseBytes) &
                             (~NSByteCountFormatterUseKB);
    formatter.countStyle = NSByteCountFormatterCountStyleMemory;
    NSString* formattedSize = [formatter stringFromByteCount:cacheSizeBytes];

    return time_range == browsing_data::TimePeriod::ALL_TIME
               ? formattedSize
               : l10n_util::GetNSStringF(
                     IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                     base::SysNSStringToUTF16(formattedSize));
  }

  if (prefName == browsing_data::prefs::kCloseTabs) {
    const TabsCounter::TabsResult* tabsResult =
        static_cast<const TabsCounter::TabsResult*>(&result);
    browsing_data::BrowsingDataCounter::ResultInt tabsCount =
        tabsResult->Value();
    int windowsCount = tabsResult->window_count();
    if (tabsCount > 0 && windowsCount > 1) {
      std::u16string tabs_counter_string =
          l10n_util::GetPluralStringFUTF16(IDS_TABS_COUNT, tabsCount);
      std::u16string windows_counter_string =
          l10n_util::GetPluralStringFUTF16(IDS_WINDOWS_COUNT, windowsCount);
      return l10n_util::GetNSStringF(IDS_DEL_TABS_MULTIWINDOW_COUNTER,
                                     tabs_counter_string,
                                     windows_counter_string);
    } else {
      return l10n_util::GetPluralNSStringF(IDS_DEL_TABS_COUNTER, tabsCount);
    }
  }

  return base::SysUTF16ToNSString(
      browsing_data::GetCounterTextFromResult(&result));
}

}  // namespace quick_delete_util
