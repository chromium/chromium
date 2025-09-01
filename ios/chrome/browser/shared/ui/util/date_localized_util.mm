// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/date_localized_util.h"

#import "base/i18n/time_formatting.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"

namespace date_localized {

std::u16string GetRelativeDateLocalized(const base::Time& visit_time) {
  std::u16string date_str = ui::TimeFormat::RelativeDate(
      visit_time, base::Time::Now().LocalMidnight());
  if (date_str.empty()) {
    date_str = base::TimeFormatFriendlyDate(visit_time);
  } else {
    date_str = l10n_util::GetStringFUTF16(
        IDS_HISTORY_DATE_WITH_RELATIVE_TIME, date_str,
        base::TimeFormatFriendlyDate(visit_time));
  }
  return date_str;
}

}  // namespace date_localized
