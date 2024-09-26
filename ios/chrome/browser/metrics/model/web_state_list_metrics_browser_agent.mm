// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/web_state_list_metrics_browser_agent.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/navigation_metrics/navigation_metrics.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(WebStateListMetricsBrowserAgent)

WebStateListMetricsBrowserAgent::WebStateListMetricsBrowserAgent(
    Browser* browser,
    SessionMetrics* session_metrics)
    : web_state_list_(browser->GetWebStateList()),
      session_metrics_(session_metrics) {
  DCHECK(web_state_list_);
  DCHECK(session_metrics_);
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
  web_state_forwarder_ =
      std::make_unique<AllWebStateObservationForwarder>(web_state_list_, this);

  ProfileIOS* profile = browser->GetProfile();
  session_restoration_service_observation_.Observe(
      SessionRestorationServiceFactory::GetForProfile(profile));

  is_off_record_ = profile->IsOffTheRecord();
  is_inactive_ = browser->IsInactive();
}

WebStateListMetricsBrowserAgent::~WebStateListMetricsBrowserAgent() = default;

#pragma mark - SessionRestorationObserver

void WebStateListMetricsBrowserAgent::WillStartSessionRestoration(
    Browser* browser) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser->GetWebStateList() != web_state_list_) {
    return;
  }

  metric_collection_paused_ = true;
}

void WebStateListMetricsBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser->GetWebStateList() != web_state_list_) {
    return;
  }

  metric_collection_paused_ = false;
}

#pragma mark - WebStateListObserver

void WebStateListMetricsBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (metric_collection_paused_) {
    return;
  }

  base::TimeDelta age_at_deletion =
      base::Time::Now() - detach_change.detached_web_state()->GetCreationTime();
  base::UmaHistogramCustomTimes("Tab.AgeAtDeletion", age_at_deletion,
                                base::Minutes(1), base::Days(24), 50);

  if (detach_change.is_user_action()) {
    base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  }
}

void WebStateListMetricsBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (web_state_list->IsBatchInProgress()) {
    return;
  }
  if (metric_collection_paused_) {
    return;
  }

  if (status.active_web_state_change()) {
    session_metrics_->OnWebStateActivated();
    if (change.type() == WebStateListChange::Type::kReplace) {
      return;
    }

    base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
  }
  switch (change.type()) {
    case WebStateListChange::Type::kInsert:
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kMove: {
      UpdateCrashkeysTabCount();
      break;
    }
    case WebStateListChange::Type::kStatusOnly:
    case WebStateListChange::Type::kReplace:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      break;
  }
}

void WebStateListMetricsBrowserAgent::BatchOperationEnded(
    WebStateList* web_state_list) {
  if (metric_collection_paused_) {
    return;
  }
  UpdateCrashkeysTabCount();
}

#pragma mark - WebStateObserver

void WebStateListMetricsBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted()) {
    return;
  }

  if (!navigation_context->IsSameDocument() &&
      !web_state->GetBrowserState()->IsOffTheRecord()) {
    int tab_count = static_cast<int>(web_state_list_->count());
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);
  }

  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  navigation_metrics::RecordPrimaryMainFrameNavigation(
      item ? item->GetVirtualURL() : GURL(),
      navigation_context->IsSameDocument(),
      web_state->GetBrowserState()->IsOffTheRecord(),
      profile_metrics::GetBrowserProfileType(web_state->GetBrowserState()));
}

void WebStateListMetricsBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  base::UmaHistogramBoolean("Tab.HasThemeColor",
                            web_state->GetThemeColor() != nil);
  // By default the underpage color is white. Only consider color as custom if
  // it is not white.
  CGFloat red = 1;
  CGFloat green = 1;
  CGFloat blue = 1;
  CGFloat alpha = 1;
  UIColor* color = web_state->GetUnderPageBackgroundColor();
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  BOOL isWhite = red > 0.999 && green > 0.999 && blue > 0.999 && alpha > 0.99;
  base::UmaHistogramBoolean("Tab.HasCustomUnderPageBackgroundColor", isWhite);
}

#pragma mark - BrowserObserver

void WebStateListMetricsBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);

  session_restoration_service_observation_.Reset();

  web_state_forwarder_.reset(nullptr);
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;

  browser->RemoveObserver(this);
}

void WebStateListMetricsBrowserAgent::UpdateCrashkeysTabCount() {
  if (is_off_record_) {
    crash_keys::SetIncognitoTabCount(web_state_list_->count());
    return;
  }
  if (is_inactive_) {
    crash_keys::SetInactiveTabCount(web_state_list_->count());
    return;
  }

  crash_keys::SetRegularTabCount(web_state_list_->count());
}
