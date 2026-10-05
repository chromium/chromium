// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string_view>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
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

constexpr std::string_view kTaskURL =
    "https://artsandculture.google.com/story/"
    "james-tissot-painter-of-the-gilded-age/eAXhY-F_v4hI8g?hl=en";

}  // namespace

// Tab helper that manages observing the newly opened `WebState` until page load
// completes, then presents the page action menu in-product help.
class GeminiTaskTabHelper : public web::WebStateUserData<GeminiTaskTabHelper>,
                            public web::WebStateObserver {
 public:
  ~GeminiTaskTabHelper() override {
    if (web_state_) {
      web_state_->RemoveObserver(this);
    }
  }

  // `web::WebStateObserver` implementation:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override {
    if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
      ShowIPH();
    }
    SelfDestruct();
  }

  void WebStateDestroyed(web::WebState* web_state) override {
    web_state_ = nullptr;
  }

 private:
  friend class web::WebStateUserData<GeminiTaskTabHelper>;

  GeminiTaskTabHelper(web::WebState* web_state, CommandDispatcher* dispatcher)
      : web_state_(web_state), dispatcher_(dispatcher) {
    CHECK(web_state_);
    web_state_->AddObserver(this);
  }

  void ShowIPH() {
    if (!dispatcher_) {
      return;
    }
    id<HelpCommands> handler = HandlerForProtocol(dispatcher_, HelpCommands);
    [handler presentInProductHelpWithType:InProductHelpType::kPageActionMenu];
  }

  void SelfDestruct() {
    if (web_state_) {
      RemoveFromWebState(web_state_);
    }
  }

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak CommandDispatcher* dispatcher_ = nil;
};

class GeminiTaskInfo : public TaskInfo {
 public:
  GeminiTaskInfo() = default;
  ~GeminiTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kGemini; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_GEMINI);
  }
  std::string GetTaskDescription() const override {
    return l10n_util::GetStringUTF8(
        IDS_IOS_LEVEL_UP_FEATURE_GEMINI_DESCRIPTION);
  }
  Symbol GetIconSymbol() const override {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    return SymbolGeminiBrandedLogo;
#else
    return SymbolGeminiNonBrandedLogo;
#endif
  }
  bool IsMulticolorIcon() const override {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    return true;
#else
    return false;
#endif
  }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override {
    return "MobileGeminiPromptSent";
  }
  std::string GetCompletionSnackbarMessage() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_TASK_COMPLETED_GEMINI);
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::BindRepeating(^(CommandDispatcher* dispatcher,
                                 Browser* browser) {
      if (!browser) {
        return;
      }
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kTaskURL));
      UrlLoadingBrowserAgent::FromBrowser(browser)->Load(params);

      web::WebState* active_web_state =
          browser->GetWebStateList()->GetActiveWebState();
      if (active_web_state) {
        GeminiTaskTabHelper::CreateForWebState(active_web_state, dispatcher);
      }
    });
  }
};

std::unique_ptr<TaskInfo> CreateGeminiTaskInfo() {
  return std::make_unique<GeminiTaskInfo>();
}
