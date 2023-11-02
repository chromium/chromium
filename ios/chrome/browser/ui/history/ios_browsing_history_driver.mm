// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"

#import <utility>

#import "base/check.h"
#import "base/strings/utf_string_conversions.h"
#import "components/browsing_data/core/history_notice_utils.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_utils.h"
#import "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/ui/history/history_consumer.h"
#import "ios/chrome/browser/ui/history/ios_browsing_history_driver_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using history::BrowsingHistoryService;

#pragma mark - IOSBrowsingHistoryDriver

IOSBrowsingHistoryDriver::IOSBrowsingHistoryDriver(
    ChromeBrowserState* browser_state,
    IOSBrowsingHistoryDriverDelegate* delegate)
    : browser_state_(browser_state), delegate_(delegate) {
  DCHECK(browser_state_);
}

IOSBrowsingHistoryDriver::~IOSBrowsingHistoryDriver() = default;

#pragma mark - Private methods

void IOSBrowsingHistoryDriver::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  delegate_->HistoryQueryCompleted(results, query_results_info,
                                   std::move(continuation_closure));
}

void IOSBrowsingHistoryDriver::OnRemoveVisitsComplete() {
  // Ignored.
}

void IOSBrowsingHistoryDriver::OnRemoveVisitsFailed() {
  // Ignored.
}

void IOSBrowsingHistoryDriver::OnRemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list) {
  // Ignored.
}

void IOSBrowsingHistoryDriver::HistoryDeleted() {
  delegate_->HistoryWasDeleted();
}

void IOSBrowsingHistoryDriver::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms,
    bool has_synced_results) {
  delegate_->ShowNoticeAboutOtherFormsOfBrowsingHistory(has_other_forms);
}

bool IOSBrowsingHistoryDriver::AllowHistoryDeletions() {
  // Current reasons for suppressing history deletions are from features that
  // are not currently supported on iOS. Reasons being administrator policy and
  // supervised users.
  return true;
}

bool IOSBrowsingHistoryDriver::ShouldHideWebHistoryUrl(const GURL& url) {
  return !ios::CanAddURLToHistory(url);
}

history::WebHistoryService* IOSBrowsingHistoryDriver::GetWebHistoryService() {
  return ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_);
}

void IOSBrowsingHistoryDriver::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::OnceCallback<void(bool)> callback) {
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service, history_service, std::move(callback));
}
