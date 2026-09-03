// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr char kTaskURL[] = "https://artsandculture.google.com/color?col=RED";

}  // namespace

// Tab helper that manages observing the newly opened WebState until page load
// completes, then launches the Lens UI.
class LensWebsiteSearchTaskTabHelper
    : public web::WebStateUserData<LensWebsiteSearchTaskTabHelper>,
      public web::WebStateObserver {
 public:
  ~LensWebsiteSearchTaskTabHelper() override {
    if (web_state_) {
      web_state_->RemoveObserver(this);
    }
  }

  // web::WebStateObserver implementation:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override {
    if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
      ShowLensUI();
    }
    SelfDestruct();
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    web_state_ = nullptr;
  }

 private:
  friend class web::WebStateUserData<LensWebsiteSearchTaskTabHelper>;

  LensWebsiteSearchTaskTabHelper(web::WebState* web_state,
                                 CommandDispatcher* dispatcher)
      : web_state_(web_state), dispatcher_(dispatcher) {
    CHECK(web_state_);
    web_state_->AddObserver(this);
  }

  void ShowLensUI() {
    if (dispatcher_) {
      id<LensOverlayCommands> lensOverlayHandler =
          HandlerForProtocol(dispatcher_, LensOverlayCommands);
      [lensOverlayHandler createAndShowLensUI:YES
                                   entrypoint:LensOverlayEntrypoint::kLevelUp
                                   completion:nil];
    }
  }

  void SelfDestruct() {
    if (web_state_) {
      RemoveFromWebState(web_state_);
    }
  }

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak CommandDispatcher* dispatcher_ = nil;
};

class LensWebsiteSearchTaskInfo : public TaskInfo {
 public:
  LensWebsiteSearchTaskInfo() = default;
  ~LensWebsiteSearchTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kLensWebsiteSearch; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_GOOGLE_LENS);
  }
  std::string GetTaskDescription() const override {
    return "Draw, highlight, or tap to search and get results without leaving "
           "your tab";
  }
  Symbol GetIconSymbol() const override { return SymbolCameraLens; }
  bool IsMulticolorIcon() const override { return true; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSearch;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_TASK_COMPLETED_LENS_WEBSITE_SEARCH);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(
        ^(CommandDispatcher* dispatcher, Browser* browser) {
          if (browser) {
            UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kTaskURL));
            UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);

            web::WebState* activeWebState =
                browser->GetWebStateList()->GetActiveWebState();
            if (activeWebState) {
              LensWebsiteSearchTaskTabHelper::CreateForWebState(activeWebState,
                                                                dispatcher);
            }
          }
        });
  }
};

std::unique_ptr<TaskInfo> CreateLensWebsiteSearchTaskInfo() {
  return std::make_unique<LensWebsiteSearchTaskInfo>();
}
