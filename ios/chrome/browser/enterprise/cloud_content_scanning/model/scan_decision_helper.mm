// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/scan_decision_helper.h"

#import "base/notreached.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace enterprise_connectors {

namespace {

// Finds the Browser that indirectly owns the WebState. Returns nullptr if no
// Browser is found.
Browser* GetBrowserForWebState(web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  if (!profile) {
    return nullptr;
  }
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  BrowserAndIndex browser_and_index =
      FindBrowserAndIndex(web_state->GetUniqueIdentifier(),
                          browser_list->BrowsersOfType(
                              BrowserList::BrowserType::kRegularAndIncognito));

  int tab_index = browser_and_index.tab_index;
  Browser* browser = browser_and_index.browser;
  if (tab_index == WebStateList::kInvalidIndex || !browser) {
    return nullptr;
  }
  return browser;
}

// Finds the commands handler and dispatch the enterprise warning dialog command
// if handler exist. Configure the dialog type based on trigger type.
void DispatchWarningDialogCommands(
    web::WebState* web_state,
    TriggerType trigger_type,
    base::OnceCallback<void(bool)> download_proceed) {
  Browser* browser = GetBrowserForWebState(web_state);

  // Fail to get the browser instance and cannot show warning dialog, we
  // just blocked the download.
  if (!browser) {
    std::move(download_proceed).Run(false);
    return;
  }

  id<EnterpriseCommands> commands_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), EnterpriseCommands);

  // Something went wrong and we fail to get the warning dialog
  // commands_handler, block the download.
  if (!commands_handler) {
    std::move(download_proceed).Run(false);
    return;
  }

  enterprise::DialogType dialog_type;
  switch (trigger_type) {
    case TriggerType::kSavePrompt:
      dialog_type = enterprise::DialogType::kDownloadSaveWarn;
      break;
    case TriggerType::kShareSheet:
      dialog_type = enterprise::DialogType::kDownloadShareWarn;
      break;
  }
  [commands_handler showEnterpriseWarningDialog:dialog_type
                             organizationDomain:std::string()
                                       callback:std::move(download_proceed)];
}

// Finds the snackbar handler and dispatch the snackbar command if handler
// exist. Configure the snackbar message based on trigger type.
void DispatchSnackbarCommands(web::WebState* web_state,
                              TriggerType trigger_type) {
  Browser* browser = GetBrowserForWebState(web_state);

  if (!browser) {
    return;
  }
  id<SnackbarCommands> snackbar_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);

  // Only tries to show the snackbar message if we can find the Snackbar
  // Handler.
  if (snackbar_handler) {
    NSString* title;
    switch (trigger_type) {
      case TriggerType::kSavePrompt:
        title = l10n_util::GetNSString(
            IDS_IOS_ENTERPRISE_FILE_SAVE_BLOCKED_SNACKBAR_TEXT);
        break;
      case TriggerType::kShareSheet:
        title = l10n_util::GetNSString(
            IDS_IOS_ENTERPRISE_FILE_SHARE_BLOCKED_SNACKBAR_TEXT);
        break;
    }
    SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
    [snackbar_handler showSnackbarMessageAfterDismissingKeyboard:message];
  }
}

}  // namespace

void HandleScanDecision(web::WebState* web_state,
                        RequestHandlerResult result,
                        TriggerType trigger_type,
                        base::OnceCallback<void(bool)> download_proceed) {
  switch (result.final_result) {
    case FinalContentAnalysisResult::SUCCESS:
      std::move(download_proceed).Run(true);
      break;
    case FinalContentAnalysisResult::WARNING:
      DispatchWarningDialogCommands(web_state, trigger_type,
                                    std::move(download_proceed));
      break;
    case FinalContentAnalysisResult::LARGE_FILES:
    case FinalContentAnalysisResult::FAILURE:
    case FinalContentAnalysisResult::FAIL_CLOSED:
      DispatchSnackbarCommands(web_state, trigger_type);

      // Block the download even if we fail to show the snackbar message to
      // users.
      std::move(download_proceed).Run(false);
      break;
    case FinalContentAnalysisResult::ENCRYPTED_FILES:
    case FinalContentAnalysisResult::FORCE_SAVE_TO_CLOUD:
      // Force Save to Cloud and Encrypted Files are not supported on iOS.
      NOTREACHED();
  }
}

}  // namespace enterprise_connectors
