// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"

#import <Foundation/Foundation.h>

#import "base/strings/string_util.h"
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/alert_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/app_launcher_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_alert_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_confirm_dialog_overlay.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/java_script_prompt_dialog_overlay.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

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
      ->GetForProfile(browser_->GetProfile())
      ->AddEvent(event);
}

#pragma mark - WebStateListObserver

void BreadcrumbManagerBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      if (!status.active_web_state_change()) {
        return;
      }
      std::optional<int> old_tab_id =
          status.old_active_web_state
              ? std::optional<int>(GetTabId(status.old_active_web_state))
              : std::nullopt;
      std::optional<int> new_tab_id =
          status.new_active_web_state
              ? std::optional<int>(GetTabId(status.new_active_web_state))
              : std::nullopt;
      LogActiveTabChanged(old_tab_id, new_tab_id,
                          web_state_list->active_index());
      break;
    }
    case WebStateListChange::Type::kDetach: {
      if (batch_operation_) {
        ++batch_operation_->close_count;
        return;
      }
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      LogTabClosedAt(GetTabId(detach_change.detached_web_state()),
                     detach_change.detached_from_index());
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& move_change =
          change.As<WebStateListChangeMove>();
      LogTabMoved(GetTabId(move_change.moved_web_state()),
                  move_change.moved_from_index(), move_change.moved_to_index());
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      LogTabReplaced(GetTabId(replace_change.replaced_web_state()),
                     GetTabId(replace_change.inserted_web_state()),
                     replace_change.index());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      if (batch_operation_) {
        ++batch_operation_->insertion_count;
        return;
      }
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      LogTabInsertedAt(GetTabId(insert_change.inserted_web_state()),
                       insert_change.index(), status.active_web_state_change());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // TODO(crbug.com/330155206): Log tab group creation in breadcrumbs.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // TODO(crbug.com/330155206): Log tab group's visual data update in
      // breadcrumbs.
      break;
    case WebStateListChange::Type::kGroupMove: {
      // TODO(crbug.com/330155206): Should we just record a group move instead
      // of N tab moves?
      const WebStateListChangeGroupMove& group_move_change =
          change.As<WebStateListChangeGroupMove>();
      const TabGroupRange from_range = group_move_change.moved_from_range();
      const TabGroupRange to_range = group_move_change.moved_to_range();
      CHECK_EQ(from_range.count(), to_range.count());
      const int count = from_range.count();
      for (int offset = 0; offset < count; ++offset) {
        const int from_index = from_range.range_begin() + offset;
        const int to_index = to_range.range_begin() + offset;
        LogTabMoved(GetTabId(web_state_list->GetWebStateAt(to_index)),
                    from_index, to_index);
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete:
      // TODO(crbug.com/330155206): Log tab group deletion in breadcrumbs.
      break;
  }
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
    NOTREACHED_IN_MIGRATION();  // Missing breadcrumbs for the dialog.
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
