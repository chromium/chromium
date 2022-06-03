// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_util.h"

#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace history {

std::u16string GetRelativeDateLocalized(const base::Time& visit_time) {
  base::Time midnight = base::Time::Now().LocalMidnight();
  std::u16string date_str = ui::TimeFormat::RelativeDate(visit_time, &midnight);
  if (date_str.empty()) {
    date_str = base::TimeFormatFriendlyDate(visit_time);
  } else {
    date_str = l10n_util::GetStringFUTF16(
        IDS_HISTORY_DATE_WITH_RELATIVE_TIME, date_str,
        base::TimeFormatFriendlyDate(visit_time));
  }
  return date_str;
}

NSString* FormattedTitle(const std::u16string& title, const GURL& url) {
  // Use url as title if no title.
  bool using_url_as_the_title = false;
  std::u16string formatted_title(title);
  if (title.empty()) {
    using_url_as_the_title = true;
    formatted_title = url_formatter::FormatUrl(url);
  }
  // Since the title can contain BiDi text, mark the text as either RTL or LTR,
  // depending on the characters in the string. If the URL is used as the title,
  // mark the title as LTR since URLs are always treated as left to right
  // strings.
  if (base::i18n::IsRTL()) {
    if (using_url_as_the_title)
      base::i18n::WrapStringWithLTRFormatting(&formatted_title);
    else
      base::i18n::AdjustStringForLocaleDirection(&formatted_title);
  }
  return base::SysUTF16ToNSString(formatted_title);
}

}  // namespace history
