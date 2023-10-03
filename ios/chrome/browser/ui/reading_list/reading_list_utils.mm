// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace reading_list {
namespace {

// Helper function invoked to add an entry to reading list once the canonical
// URL for the page has been fetched asynchronously.
void OnCanonicalUrlFetched(ReadingListBrowserAgent* readingListBrowserAgent,
                           NSString* title,
                           const GURL& visible_url,
                           const GURL& canonical_url) {
  const GURL& url = canonical_url.is_valid() ? canonical_url : visible_url;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:url title:title];
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

}  // namespace

ReadingListUIDistillationStatus UIStatusFromModelStatus(
    ReadingListEntry::DistillationState distillation_state) {
  switch (distillation_state) {
    case ReadingListEntry::WILL_RETRY:
    case ReadingListEntry::PROCESSING:
    case ReadingListEntry::WAITING:
      return ReadingListUIDistillationStatusPending;
    case ReadingListEntry::PROCESSED:
      return ReadingListUIDistillationStatusSuccess;
    case ReadingListEntry::DISTILLATION_ERROR:
      return ReadingListUIDistillationStatusFailure;
  }
}

void AddToReadingListUsingCanonicalUrl(
    ReadingListBrowserAgent* readingListBrowserAgent,
    web::WebState* web_state) {
  DCHECK(web_state);
  activity_services::RetrieveCanonicalUrl(
      web_state, base::BindOnce(&OnCanonicalUrlFetched, readingListBrowserAgent,
                                base::SysUTF16ToNSString(web_state->GetTitle()),
                                web_state->GetVisibleURL()));
}

}  // namespace reading_list
