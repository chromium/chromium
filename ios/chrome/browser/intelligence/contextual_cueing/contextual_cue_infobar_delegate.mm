// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

namespace {

id<GeminiCommands> GetGeminiHandlerForWebState(web::WebState* web_state) {
  if (!web_state) {
    return nil;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  if (!profile) {
    return nil;
  }
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  if (!browser_list) {
    return nil;
  }
  BrowserAndIndex browser_and_index =
      FindBrowserAndIndex(web_state->GetUniqueIdentifier(),
                          browser_list->BrowsersOfType(
                              BrowserList::BrowserType::kRegularAndIncognito));
  Browser* browser = browser_and_index.browser;
  if (!browser) {
    return nil;
  }
  return HandlerForProtocol(browser->GetCommandDispatcher(), GeminiCommands);
}

}  // namespace

// static
bool ContextualCueInfobarDelegate::Create(web::WebState* web_state,
                                          id<GeminiCommands> gemini_handler) {
  if (!web_state) {
    return false;
  }

  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return false;
  }

  const std::optional<optimization_guide::proto::ContextualCue>& cue_opt =
      tab_helper->GetContextualCue();
  if (!cue_opt.has_value()) {
    return false;
  }

  const optimization_guide::proto::ContextualCue& cue = *cue_opt;
  if (!cue.has_gemini_in_chrome_surface() || !cue.has_anchored_message_cue()) {
    return false;
  }

  // TODO(crbug.com/559227915): Refactor the output of ContextualCueingTabHelper
  // to expose a cleaner model object with generic fields (title, action_text,
  // subtitle, prompt) instead of exposing Optimization Guide proto internals.
  const auto& anchored_cue = cue.anchored_message_cue();
  std::u16string title =
      base::UTF8ToUTF16(anchored_cue.anchored_message_text());
  std::u16string button_text = base::UTF8ToUTF16(anchored_cue.action_text());
  std::string prompt = cue.gemini_in_chrome_surface().prompt();

  // Do not show the infobar banner if any required display or prompt text is
  // missing or empty.
  if (title.empty() || button_text.empty() || prompt.empty()) {
    return false;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!infobar_manager) {
    return false;
  }

  // TODO(crbug.com/559227915): Move FET presentation gating out of delegate
  // creation into ContextualCueingTabHelper.
  // Atomically check Feature Engagement Tracker triggering conditions and
  // rate limits. If FET rejects the promo, do not display the banner.
  if (!tab_helper->RecordCueShown()) {
    return false;
  }

  // Remove any existing contextual cue infobar before adding a new one.
  Remove(web_state);

  ContextualCueInfobarConfig config{std::move(title), std::move(button_text),
                                    std::move(prompt)};
  auto delegate = std::make_unique<ContextualCueInfobarDelegate>(
      web_state, gemini_handler, std::move(config));

  auto infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeContextualCue, std::move(delegate));
  return infobar_manager->AddInfoBar(std::move(infobar)) != nullptr;
}

// static
void ContextualCueInfobarDelegate::Remove(web::WebState* web_state) {
  if (!web_state) {
    return;
  }
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!infobar_manager) {
    return;
  }

  for (infobars::InfoBar* infobar : infobar_manager->infobars()) {
    if (infobar->delegate()->GetIdentifier() ==
        CONTEXTUAL_CUE_INFOBAR_DELEGATE_IOS) {
      infobar_manager->RemoveInfoBar(infobar);
      break;
    }
  }
}

ContextualCueInfobarDelegate::ContextualCueInfobarDelegate(
    web::WebState* web_state,
    id<GeminiCommands> gemini_handler,
    ContextualCueInfobarConfig config)
    : web_state_(web_state),
      gemini_handler_(gemini_handler),
      config_(std::move(config)) {}

ContextualCueInfobarDelegate::~ContextualCueInfobarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
ContextualCueInfobarDelegate::GetIdentifier() const {
  return CONTEXTUAL_CUE_INFOBAR_DELEGATE_IOS;
}

std::u16string ContextualCueInfobarDelegate::GetTitleText() const {
  return config_.title;
}

std::u16string ContextualCueInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

int ContextualCueInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ContextualCueInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  CHECK_EQ(button, BUTTON_OK);
  return config_.button_text;
}

ui::ImageModel ContextualCueInfobarDelegate::GetIcon() const {
  UIImage* symbol_image =
      SymbolWithPointSize(SymbolSparkles, kInfobarSymbolPointSize);
  return ui::ImageModel::FromImage(gfx::Image(symbol_image));
}

bool ContextualCueInfobarDelegate::UseIconBackgroundTint() const {
  return false;
}

bool ContextualCueInfobarDelegate::Accept() {
  if (!web_state_) {
    return true;
  }

  has_user_interacted_ = true;

  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->RecordCueClicked();
  }

  id<GeminiCommands> handler = gemini_handler_;
  if (!handler) {
    handler = GetGeminiHandlerForWebState(web_state_);
  }

  if (handler) {
    GeminiStartupState* startup_state = [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::ContextualCueInfobar];
    if (!config_.prompt.empty()) {
      startup_state.prepopulatedPrompt =
          base::SysUTF8ToNSString(config_.prompt);
    }
    startup_state.shouldAutoSubmit = YES;
    [handler startGeminiEntryFlowWithStartupState:startup_state
                               baseViewController:nil
                         showSnackbarOnCompletion:YES
                                       completion:nil];
  }

  return true;
}

void ContextualCueInfobarDelegate::InfoBarDismissed() {
  // Ignored if the user already interacted (e.g. accepted the cue), as the
  // banner animates away after tapping the action button and triggers
  // InfoBarDismissed(), which would otherwise double-count user interactions.
  if (has_user_interacted_) {
    return;
  }
  has_user_interacted_ = true;

  if (!web_state_) {
    return;
  }

  ContextualCueingTabHelper* tab_helper =
      ContextualCueingTabHelper::FromWebState(web_state_);
  if (tab_helper) {
    tab_helper->RecordCueDismissed();
  }
}

bool ContextualCueInfobarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return details.is_navigation_to_different_page;
}

}  // namespace contextual_cueing
