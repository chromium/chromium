// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_util.h"

#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

NSString* GetReadingListCellAccessibilityLabel(
    NSString* title,
    NSString* subtitle,
    ReadingListUIDistillationStatus distillation_status,
    BOOL showCloudSlashIcon) {
  int state_string_id =
      distillation_status == ReadingListUIDistillationStatusSuccess
          ? IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_DOWNLOADED
          : IDS_IOS_READING_LIST_ACCESSIBILITY_STATE_NOT_DOWNLOADED;
  NSString* a11y_state = l10n_util::GetNSString(state_string_id);
  int base_string_id =
      showCloudSlashIcon
          ? IDS_IOS_READING_LIST_ENTRY_WITH_UPLOAD_STATE_ACCESSIBILITY_LABEL
          : IDS_IOS_READING_LIST_ENTRY_ACCESSIBILITY_LABEL;

  return l10n_util::GetNSStringF(
      base_string_id, base::SysNSStringToUTF16(title),
      base::SysNSStringToUTF16(a11y_state), base::SysNSStringToUTF16(subtitle));
}

NSString* GetReadingListCellDistillationDateText(int64_t distillation_date) {
  if (!distillation_date)
    return nil;
  int64_t now = (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
  int64_t elapsed = now - distillation_date;
  if (elapsed < base::Time::kMicrosecondsPerMinute) {
    // This will also catch items added in the future. In that case, show the
    // "just now" string.
    return l10n_util::GetNSString(IDS_IOS_READING_LIST_JUST_NOW);
  } else {
    return base::SysUTF16ToNSString(ui::TimeFormat::SimpleWithMonthAndYear(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
        base::Microseconds(elapsed), true));
  }
}

BOOL AreReadingListListItemsEqual(id<ReadingListListItem> first,
                                  id<ReadingListListItem> second) {
  if (first == second)
    return YES;
  if (!first || !second || ![second isKindOfClass:[first class]])
    return NO;
  return [first.title isEqualToString:second.title] &&
         first.entryURL.host() == second.entryURL.host() &&
         first.distillationState == second.distillationState &&
         [first.distillationDateText
             isEqualToString:second.distillationDateText] &&
         first.showCloudSlashIcon == second.showCloudSlashIcon;
}
