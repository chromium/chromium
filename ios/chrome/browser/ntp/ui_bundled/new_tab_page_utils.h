// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_

#include "base/time/time.h"
#include "url/gurl.h"

class TemplateURLService;

// Whether the top of feed sync promo has met the criteria to be shown.
bool ShouldShowTopOfFeedSyncPromo();

// Retrieves the URL for the AIM web page. `query_start_time` is the time that
// the user clicked the submit button.
GURL GetUrlForAim(TemplateURLService* turl_service,
                  const base::Time& query_start_time);

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_UTILS_H_
