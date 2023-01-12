// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/sharing/activity_services/canonical_url_retriever.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace reading_list {
namespace {

// Helper struct to weakly capture the handler (as base::Bind cannot express
// Objective-C weak pointers).
struct WeakHandler {
  __weak id<BrowserCommands> value;
};

// Helper function invoked to add an entry to reading list once the canonical
// URL for the page has been fetched asynchronously.
void OnCanonicalUrlFetched(WeakHandler weak_handler,
                           NSString* title,
                           const GURL& visible_url,
                           const GURL& canonical_url) {
  const GURL& url = canonical_url.is_valid() ? canonical_url : visible_url;
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return;

  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:url title:title];

  [weak_handler.value addToReadingList:command];
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

void AddToReadingListUsingCanonicalUrl(id<BrowserCommands> handler,
                                       web::WebState* web_state) {
  DCHECK(web_state);
  activity_services::RetrieveCanonicalUrl(
      web_state, base::BindOnce(&OnCanonicalUrlFetched, WeakHandler{handler},
                                base::SysUTF16ToNSString(web_state->GetTitle()),
                                web_state->GetVisibleURL()));
}

}  // namespace reading_list
