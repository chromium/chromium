// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_DATE_LOCALIZED_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_DATE_LOCALIZED_UTIL_H_

#include <string>

namespace base {
class Time;
}

namespace date_localized {

// Returns a localized version of `visit_time` including a relative
// indicator (e.g. today, yesterday).
std::u16string GetRelativeDateLocalized(const base::Time& visit_time);

}  // namespace date_localized

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_DATE_LOCALIZED_UTIL_H_
