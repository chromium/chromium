// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"

#import <Foundation/Foundation.h>

#import "base/strings/string_util.h"
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirm_dialog_overlay.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_dialog_overlay.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

int GetTabId(const web::WebState* const web_state) {
  return BreadcrumbManagerTabHelper::FromWebState(web_state)->GetUniqueId();
}

}  // namespace

const char kBreadcrumbOverlay[] = "Overlay";
const char kBreadcrumbOverlayActivated[] = "#activated";
const char kBreadcrumbOverlayHttpAuth[] = "#http-auth";
const char kBreadcrumbOverlayAlert[] = "#alert";
const char kBreadcrumbOverlayAppLaunch[] = "#app-launch";
const char kBreadcrumbOverlayJsAlert[] = "#js-alert";
const char kBreadcrumbOverlayJsConfirm[] = "#js-confirm";
const char kBreadcrumbOverlayJsPrompt[] = "#js-prompt";

BROWSER_USER_DATA_KEY_IMPL(BreadcrumbManagerBrowserAgent)

BreadcrumbManagerBrowserAgent::BreadcrumbManagerBrowserAgent(Browser* browser)
    : browser_(browser) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);

  overlay_observation_.Observe(
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea));
}

BreadcrumbManagerBrowserAgent::~BreadcrumbManagerBrowserAgent() = default;

void BreadcrumbManagerBrowserAgent::BrowserDestroyed(Browser* browser) {
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
}

void BreadcrumbManagerBrowserAgent::PlatformLogEvent(const std::string& event) {
  BreadcrumbManagerKeyedServiceFactory::GetInstance()
      ->GetForBrowserState(browser_->GetBrowserState())
      ->AddEvent(event);
}

void BreadcrumbManagerBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (batch_operation_) {
    ++batch_operation_->insertion_count;
    return;
  }
  LogTabInsertedAt(GetTabId(web_state), index, activating);
}

void BreadcrumbManagerBrowserAgent::WebStateMoved(WebStateList* web_state_list,
                                                  web::WebState* web_state,
                                                  int from_index,
                                                  int to_index) {
  LogTabMoved(GetTabId(web_state), from_index, to_index);
}

void BreadcrumbManagerBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  LogTabReplaced(GetTabId(old_web_state), GetTabId(new_web_state), index);
}

void BreadcrumbManagerBrowserAgent::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  if (batch_operation_) {
    ++batch_operation_->close_count;
    return;
  }
  LogTabClosedAt(GetTabId(web_state), index);
}

void BreadcrumbManagerBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (reason != ActiveWebStateChangeReason::Activated)
    return;
  absl::optional<int> old_tab_id =
      old_web_state ? absl::optional<int>(GetTabId(old_web_state))
                    : absl::nullopt;
  absl::optional<int> new_tab_id =
      new_web_state ? absl::optional<int>(GetTabId(new_web_state))
                    : absl::nullopt;
  LogActiveTabChanged(old_tab_id, new_tab_id, active_index);
}

void BreadcrumbManagerBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  batch_operation_ = std::make_unique<BatchOperation>();
}

void BreadcrumbManagerBrowserAgent::BatchOperationEnded(
    WebStateList* web_state_list) {
  if (batch_operation_) {
    if (batch_operation_->insertion_count > 0) {
      LogTabsInserted(batch_operation_->insertion_count);
    }
    if (batch_operation_->close_count > 0) {
      LogTabsClosed(batch_operation_->close_count);
    }
  }
  batch_operation_.reset();
}

void BreadcrumbManagerBrowserAgent::WillShowOverlay(OverlayPresenter* presenter,
                                                    OverlayRequest* request,
                                                    bool initial_presentation) {
  std::vector<std::string> event = {kBreadcrumbOverlay};
  if (request->GetConfig<HTTPAuthOverlayRequestConfig>()) {
    event.push_back(kBreadcrumbOverlayHttpAuth);
  } else if (request->GetConfig<
                 app_launcher_overlays::AppLaunchConfirmationRequest>()) {
    event.push_back(kBreadcrumbOverlayAppLaunch);
  } else if (request->GetConfig<JavaScriptAlertDialogRequest>()) {
    event.push_back(kBreadcrumbOverlayJsAlert);
  } else if (request->GetConfig<JavaScriptConfirmDialogRequest>()) {
    event.push_back(kBreadcrumbOverlayJsConfirm);
  } else if (request->GetConfig<JavaScriptPromptDialogRequest>()) {
    event.push_back(kBreadcrumbOverlayJsPrompt);
  } else if (request->GetConfig<alert_overlays::AlertRequest>()) {
    event.push_back(kBreadcrumbOverlayAlert);
  } else {
    NOTREACHED();  // Missing breadcrumbs for the dialog.
  }

  if (!initial_presentation) {
    event.push_back(kBreadcrumbOverlayActivated);
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerBrowserAgent::OverlayPresenterDestroyed(
    OverlayPresenter* presenter) {
  DCHECK(overlay_observation_.IsObservingSource(presenter));
  overlay_observation_.Reset();
}
