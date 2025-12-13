// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <limits>
#import <utility>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "base/time/time.h"
#import "components/navigation_metrics/navigation_metrics.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace {

// Returns whether metrics should be considered for Browser of `type`.
constexpr bool ShouldRecordMetricsForBrowserOfType(Browser::Type type) {
  switch (type) {
    case Browser::Type::kRegular:
    case Browser::Type::kInactive:
    case Browser::Type::kIncognito:
      return true;

    // Ignore temporary browsers.
    case Browser::Type::kTemporary:
      return false;
  }

  NOTREACHED();
}

// Returns whether Browser of `type` is considered off-the-record.
constexpr bool IsOffTheRecord(Browser::Type type) {
  switch (type) {
    case Browser::Type::kRegular:
    case Browser::Type::kInactive:
    case Browser::Type::kTemporary:
      return false;

    case Browser::Type::kIncognito:
      return true;
  }

  NOTREACHED();
}

// Returns whether `color` is default under page background color. By default
// the color is white, so consider any non-nil color that is not white as not
// default.
bool IsDefaultUnderPageBackgroundColor(UIColor* color) {
  if (!color) {
    return true;
  }

  CGFloat red = 1.0;
  CGFloat green = 1.0;
  CGFloat blue = 1.0;
  CGFloat alpha = 1.0;
  if ([color getRed:&red green:&green blue:&blue alpha:&alpha]) {
    return red > 0.999 && green > 0.999 && blue > 0.999 && alpha > 0.99;
  }

  CGFloat white = 1.0;
  if ([color getWhite:&white alpha:&alpha]) {
    return white > 0.999 && alpha > 0.99;
  }

  return false;
}

}  // namespace

#pragma mark - TabUsageRecorderService::Helper

// Observes all WebStates for a given Browser::Type.
class TabUsageRecorderService::Helper final : public WebStateListObserver,
                                              public web::WebStateObserver {
 public:
  Helper(Browser::Type browser_type);

  Helper(const Helper&) = delete;
  Helper& operator=(const Helper&) = delete;

  ~Helper() final = default;

  // Reports the activation metric.
  void ReportActivationCount() const;

  // Clears the activation metric stats.
  void ClearActivationCount();

  // Starts observing WebStateList and all its WebStates.
  void OnWebStateListAdded(WebStateList* web_state_list);

  // Stops observing WebStateList and all its WebStates.
  void OnWebStateListRemoved(WebStateList* web_state_list);

  // WebStateListObserver:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) final;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;
  void WillBeginBatchOperation(WebStateList* web_state_list) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) final;
  void PageLoaded(web::WebState* web_state,
                  web::PageLoadCompletionStatus load_completion_status) final;

 private:
  // Updates the tabs count by `delta` and sets the crash key (unless there
  // is a batch operation in progress).
  void UpdateTabsCount(int delta);

  // Manages the observation of the WebStateList instances.
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};

  // Manages the observation of the WebState instances.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // Type of the observed Browser.
  const Browser::Type browser_type_;

  // Count how many batches operations are in progress. Some metrics are not
  // collected when this count is non-zero.
  uint32_t batch_operation_count_ = 0;

  // Number of open tabs for the given Browser type.
  int tabs_count_ = 0;

  // Number of time the active WebState changed.
  int activation_count_ = 0;
};

TabUsageRecorderService::Helper::Helper(Browser::Type browser_type)
    : browser_type_(browser_type) {
  CHECK(ShouldRecordMetricsForBrowserOfType(browser_type_));
}

void TabUsageRecorderService::Helper::ReportActivationCount() const {
  base::UmaHistogramCustomCounts("Session.OpenedTabCounts", activation_count_,
                                 /*min=*/1, /*max=*/200, /*buckets=*/50);
}

void TabUsageRecorderService::Helper::ClearActivationCount() {
  activation_count_ = 0;
}

void TabUsageRecorderService::Helper::OnWebStateListAdded(
    WebStateList* web_state_list) {
  CHECK(web_state_list->empty());
  web_state_list_observations_.AddObservation(web_state_list);
}

void TabUsageRecorderService::Helper::OnWebStateListRemoved(
    WebStateList* web_state_list) {
  // The BrowserList notifies of the Browser removal before the WebStateList
  // is emptied, so stops observing the WebStates if there are any.
  if (!web_state_list->empty()) {
    const int count = web_state_list->count();
    for (int index = 0; index < count; ++index) {
      web_state_observations_.RemoveObservation(
          web_state_list->GetWebStateAt(index));
    }

    CHECK_GT(count, 0);
    UpdateTabsCount(-count);
  }

  web_state_list_observations_.RemoveObservation(web_state_list);
}

void TabUsageRecorderService::Helper::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (batch_operation_count_ > 0) {
    // Do not report any metric if a batch operation is in progress.
    return;
  }

  if (!detach_change.is_closing()) {
    // The WebState is detached but not closed. This means it will be
    // moved to another Browser, so there is nothing to report.
    return;
  }

  web::WebState* web_state = detach_change.detached_web_state();
  base::UmaHistogramCustomTimes(
      "Tab.AgeAtDeletion", base::Time::Now() - web_state->GetCreationTime(),
      /*min=*/base::Minutes(1), /*max=*/base::Days(24), /*buckets=*/50);

  if (detach_change.is_user_action()) {
    base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  }
}

void TabUsageRecorderService::Helper::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      web_state_observations_.AddObservation(
          insert_change.inserted_web_state());
      UpdateTabsCount(+1);
      break;
    }
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web_state_observations_.RemoveObservation(
          detach_change.detached_web_state());
      UpdateTabsCount(-1);
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web_state_observations_.RemoveObservation(
          replace_change.replaced_web_state());
      web_state_observations_.AddObservation(
          replace_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kStatusOnly:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      // Not interesting.
      break;
  }

  if (batch_operation_count_ > 0) {
    // Do not report any metric if a batch operation is in progress.
    return;
  }

  if (status.active_web_state_change()) {
    ++activation_count_;
    if (change.type() != WebStateListChange::Type::kReplace) {
      base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
    }
  }
}

void TabUsageRecorderService::Helper::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  CHECK_LT(batch_operation_count_, std::numeric_limits<uint32_t>::max());
  ++batch_operation_count_;
}

void TabUsageRecorderService::Helper::BatchOperationEnded(
    WebStateList* web_state_list) {
  CHECK_GT(batch_operation_count_, 0u);
  --batch_operation_count_;

  // Force updating the crash keys.
  if (batch_operation_count_ == 0) {
    UpdateTabsCount(0);
  }
}

void TabUsageRecorderService::Helper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted()) {
    return;
  }

  const bool is_off_the_record = IsOffTheRecord(browser_type_);
  const bool is_same_document = navigation_context->IsSameDocument();
  if (!is_same_document && !is_off_the_record) {
    base::UmaHistogramCustomCounts("Tabs.TabCountPerLoad", tabs_count_,
                                   /*min=*/1, /*max=*/200, /*buckets=*/50);
  }

  navigation_metrics::RecordPrimaryMainFrameNavigation(
      web_state->GetLastCommittedURL(), is_same_document, is_off_the_record,
      is_off_the_record ? profile_metrics::BrowserProfileType::kIncognito
                        : profile_metrics::BrowserProfileType::kRegular);
}

void TabUsageRecorderService::Helper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  base::UmaHistogramBoolean("Tab.HasThemeColor", !!web_state->GetThemeColor());
  base::UmaHistogramBoolean("Tab.HasCustomUnderPageBackgroundColor",
                            !IsDefaultUnderPageBackgroundColor(
                                web_state->GetUnderPageBackgroundColor()));
}

void TabUsageRecorderService::Helper::UpdateTabsCount(int delta) {
  tabs_count_ += delta;
  CHECK_GE(tabs_count_, 0);
  CHECK_LE(tabs_count_, std::numeric_limits<int>::max());

  // Do not set the crash keys if there are batch operations in progress.
  if (batch_operation_count_ > 0) {
    return;
  }

  switch (browser_type_) {
    case Browser::Type::kRegular:
      crash_keys::SetRegularTabCount(tabs_count_);
      break;

    case Browser::Type::kIncognito:
      crash_keys::SetIncognitoTabCount(tabs_count_);
      break;

    case Browser::Type::kInactive:
      crash_keys::SetInactiveTabCount(tabs_count_);
      break;

    case Browser::Type::kTemporary:
      NOTREACHED();
  }
}

#pragma mark - TabUsageRecorderService

TabUsageRecorderService::TabUsageRecorderService(
    BrowserList* browser_list,
    SessionRestorationService* session_restoration_service) {
  browser_list_observation_.Observe(browser_list);
  session_restoration_observation_.Observe(session_restoration_service);
}

TabUsageRecorderService::~TabUsageRecorderService() = default;

void TabUsageRecorderService::RecordSessionMetrics() {
  for (auto& [_, helper] : helpers_) {
    helper->ReportActivationCount();
    helper->ClearActivationCount();
  }
}

void TabUsageRecorderService::Shutdown() {
  // Stops all observations and metrics recording.
  otr_profile_destruction_subscription_ = {};
  session_restoration_observation_.Reset();
  browser_list_observation_.Reset();
  helpers_.clear();
}

void TabUsageRecorderService::OnBrowserAdded(const BrowserList* list,
                                             Browser* browser) {
  const Browser::Type type = browser->type();
  if (!ShouldRecordMetricsForBrowserOfType(type)) {
    // Nothing to do if this type of Browser is ignored.
    return;
  }

  auto iter = helpers_.find(type);
  if (iter == helpers_.end()) {
    auto insertion_result =
        helpers_.insert(std::make_pair(type, std::make_unique<Helper>(type)));

    if (IsOffTheRecord(type)) {
      // Register a callback to be notified of the destruction of the
      // off-the-record profile, in order to clear the related metrics.
      //
      // Since the callback will be cancelled when the subscription is
      // destroyed, it is safe to use base::Unretained(this) here.
      otr_profile_destruction_subscription_ =
          browser->GetProfile()->RegisterProfileDestroyedCallback(
              base::BindOnce(&TabUsageRecorderService::OnOTRProfileDestroyed,
                             base::Unretained(this)));
    }

    CHECK(insertion_result.second);
    iter = insertion_result.first;
  }

  CHECK(iter != helpers_.end());
  CHECK(iter->second);

  WebStateList* web_state_list = browser->GetWebStateList();
  iter->second->OnWebStateListAdded(web_state_list);
}

void TabUsageRecorderService::OnBrowserRemoved(const BrowserList* list,
                                               Browser* browser) {
  const Browser::Type type = browser->type();
  if (!ShouldRecordMetricsForBrowserOfType(type)) {
    // Nothing to do if this type of Browser is ignored.
    return;
  }

  auto iter = helpers_.find(type);
  CHECK(iter != helpers_.end());
  CHECK(iter->second);

  WebStateList* web_state_list = browser->GetWebStateList();
  iter->second->OnWebStateListRemoved(web_state_list);
}

void TabUsageRecorderService::WillStartSessionRestoration(Browser* browser) {
  const Browser::Type type = browser->type();
  if (!ShouldRecordMetricsForBrowserOfType(type)) {
    // Nothing to do if this type of Browser is ignored.
    return;
  }

  auto iter = helpers_.find(type);
  CHECK(iter != helpers_.end());
  CHECK(iter->second);

  WebStateList* web_state_list = browser->GetWebStateList();
  iter->second->WillBeginBatchOperation(web_state_list);
}

void TabUsageRecorderService::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  const Browser::Type type = browser->type();
  if (!ShouldRecordMetricsForBrowserOfType(type)) {
    // Nothing to do if this type of Browser is ignored.
    return;
  }

  auto iter = helpers_.find(type);
  CHECK(iter != helpers_.end());
  CHECK(iter->second);

  WebStateList* web_state_list = browser->GetWebStateList();
  iter->second->BatchOperationEnded(web_state_list);
}

void TabUsageRecorderService::OnOTRProfileDestroyed() {
  // Drops all collected metrics about incognito tabs.
  helpers_.erase(Browser::Type::kIncognito);
}
