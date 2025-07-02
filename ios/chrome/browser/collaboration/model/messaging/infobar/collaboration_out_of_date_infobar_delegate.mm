// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_out_of_date_infobar_delegate.h"

#import "components/infobars/core/infobar_delegate.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace {

// Returns the most recently foregrounded regular browser from `browser_list`.
// If no regular browser is foregrounded, it returns the most recently
// foregrounded regular browser in the background. If no regular browser
// exists, it returns `nullptr`.
Browser* GetMostActiveSceneBrowser(BrowserList* browser_list) {
  std::set<Browser*> all_browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);

  Browser* most_active_browser = nullptr;
  for (Browser* browser_to_check : all_browsers) {
    // The pointer to the scene state is weak, so it could be nil. In that case,
    // the activation level will be 0 (lowest).
    if (most_active_browser &&
        most_active_browser->GetSceneState().activationLevel >=
            browser_to_check->GetSceneState().activationLevel) {
      continue;
    }
    most_active_browser = browser_to_check;
    if (browser_to_check->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      break;
    }
  }
  return most_active_browser;
}

}  // namespace

// static
bool CollaborationOutOfDateInfoBarDelegate::Create(ProfileIOS* profile) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  Browser* browser = GetMostActiveSceneBrowser(browser_list);
  if (!browser) {
    return false;
  }

  web::WebState* active_web_state =
      browser->GetWebStateList()->GetActiveWebState();
  if (!active_web_state) {
    return false;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(active_web_state);

  id<ApplicationCommands> application_commands_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  std::unique_ptr<CollaborationOutOfDateInfoBarDelegate> delegate =
      std::make_unique<CollaborationOutOfDateInfoBarDelegate>(
          application_commands_handler);
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeCollaborationOutOfDate, std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

CollaborationOutOfDateInfoBarDelegate::CollaborationOutOfDateInfoBarDelegate(
    id<ApplicationCommands> application_commands_handler)
    : application_commands_handler_(application_commands_handler) {}

CollaborationOutOfDateInfoBarDelegate::
    ~CollaborationOutOfDateInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
CollaborationOutOfDateInfoBarDelegate::GetIdentifier() const {
  return COLLABORATION_OUT_OF_DATE_INFOBAR_DELEGATE;
}

std::u16string CollaborationOutOfDateInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_COLLABORATION_SHARED_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_CELL_TEXT);
}

int CollaborationOutOfDateInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string CollaborationOutOfDateInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_IOS_TAB_GROUPS_PANEL_OUT_OF_DATE_MESSAGE_UPDATE_BUTTON);
}

bool CollaborationOutOfDateInfoBarDelegate::Accept() {
  [application_commands_handler_ showAppStorePage];
  return true;
}

ui::ImageModel CollaborationOutOfDateInfoBarDelegate::GetIcon() const {
  UIImage* symbolImage =
      DefaultSymbolWithPointSize(kTabGroupsSymbol, kInfobarSymbolPointSize);
  return ui::ImageModel::FromImage(gfx::Image(symbolImage));
}
