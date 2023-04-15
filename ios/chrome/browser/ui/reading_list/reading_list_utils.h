// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_

#include "components/reading_list/core/reading_list_entry.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_ui_distillation_status.h"

class ReadingListBrowserAgent;
namespace web {
class WebState;
}  // namespace web

namespace reading_list {

ReadingListUIDistillationStatus UIStatusFromModelStatus(
    ReadingListEntry::DistillationState distillation_state);

// Adds `web_state` visible or canonical URL to reading list using
// `readingListBrowserAgent`. This function retrieves the canonical URL
// asynchronously first, and post the request when the result is fetched. It
// does nothing if the `readingListBrowserAgent` or the `web_state` are
// destroyed before the URL can be added to the list.
void AddToReadingListUsingCanonicalUrl(
    ReadingListBrowserAgent* readingListBrowserAgent,
    web::WebState* web_state);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UTILS_H_
