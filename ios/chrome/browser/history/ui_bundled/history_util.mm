// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_util.h"

#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/url_formatter.h"

namespace history {

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
    if (using_url_as_the_title) {
      base::i18n::WrapStringWithLTRFormatting(&formatted_title);
    } else {
      base::i18n::AdjustStringForLocaleDirection(&formatted_title);
    }
  }
  return base::SysUTF16ToNSString(formatted_title);
}

}  // namespace history
