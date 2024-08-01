// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_UTIL_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>


namespace base {
class Time;
}
class GURL;

namespace history {

// Returns a localized version of `visit_time` including a relative
// indicator (e.g. today, yesterday).
std::u16string GetRelativeDateLocalized(const base::Time& visit_time);

// Formats `title` to support RTL, or creates an RTL supported title based on
// `url` if `title` is empty.
NSString* FormattedTitle(const std::u16string& title, const GURL& url);

}  // namespace history

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_UTIL_H_
