// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/reading_list/reading_list_constants.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(ReadingListBrowserAgent)

ReadingListBrowserAgent::ReadingListBrowserAgent(Browser* browser) {
  browser_ = browser;
}

ReadingListBrowserAgent::~ReadingListBrowserAgent() {}

#pragma mark - Public

// Adds the given urls to the reading list.
void ReadingListBrowserAgent::AddURLsToReadingList(
    NSArray<URLWithTitle*>* urls) {
  DCHECK(urls.count > 0) << "Urls are missing";

  for (URLWithTitle* url_with_title in urls) {
    AddURLToReadingListwithTitle(url_with_title.URL, url_with_title.title);
  }

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);

  ReadingListModel* reading_model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          browser_->GetBrowserState());
  CoreAccountId account_id =
      reading_model->GetAccountWhereEntryIsSavedTo(urls.lastObject.URL);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(
          browser_->GetBrowserState()->GetOriginalChromeBrowserState());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);

  NSString* snackbar_text = nil;
  MDCSnackbarMessageAction* snackbar_action = nil;
  if (!account_info.IsEmpty() &&
      base::FeatureList::IsEnabled(
          kEnableEmailInBookmarksReadingListSnackbar)) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_FOR_ACCOUNT);
    std::u16string utf16Text =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "count", (int)urls.count, "email", account_info.email);
    snackbar_text = base::SysUTF16ToNSString(utf16Text);
    snackbar_action =
        reading_list::switches::IsReadingListAccountStorageUIEnabled()
            ? CreateUndoActionWithReadingListURLs(urls)
            : nil;
  } else {
    snackbar_text =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  }

  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:snackbar_text];
  message.accessibilityLabel = snackbar_text;
  message.action = snackbar_action;
  message.duration = 2.0;

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbar_commands_handler showSnackbarMessage:message];
}

#pragma mark - Private

void ReadingListBrowserAgent::AddURLToReadingListwithTitle(const GURL& url,
                                                           NSString* title) {
  web::WebState* current_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  if (current_web_state &&
      current_web_state->GetVisibleURL().spec() == url.spec()) {
    // Log UKM if the current page is being added to Reading List.
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebStateDocument(current_web_state);
    if (source_id != ukm::kInvalidSourceId) {
      ukm::builders::IOS_PageAddedToReadingList(source_id)
          .SetAddedFromMessages(false)
          .Record(ukm::UkmRecorder::Get());
    }
  }

  base::RecordAction(base::UserMetricsAction("MobileReadingListAdd"));
  ReadingListModel* reading_model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          browser_->GetBrowserState());
  reading_model->AddOrReplaceEntry(url, base::SysNSStringToUTF8(title),
                                   reading_list::ADDED_VIA_CURRENT_APP,
                                   /*estimated_read_time=*/base::TimeDelta());
}

MDCSnackbarMessageAction*
ReadingListBrowserAgent::CreateUndoActionWithReadingListURLs(
    NSArray<URLWithTitle*>* urls) {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  base::WeakPtr<ReadingListBrowserAgent> weak_agent =
      weak_ptr_factory_.GetWeakPtr();
  action.handler = ^{
    base::RecordAction(
        base::UserMetricsAction("MobileReadingListDeleteFromSnackbarUndo"));
    ReadingListBrowserAgent* agent = weak_agent.get();
    if (agent) {
      agent->RemoveURLsFromReadingList(urls);
    }
  };
  action.accessibilityIdentifier = kReadingListAddedToAccountSnackbarUndoID;
  action.title =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_UNDO_ACTION);
  action.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_UNDO_ACTION);
  return action;
}

void ReadingListBrowserAgent::RemoveURLsFromReadingList(
    NSArray<URLWithTitle*>* urls) {
  ReadingListModel* reading_model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          browser_->GetBrowserState());

  for (URLWithTitle* url_with_title in urls) {
    reading_model->RemoveEntryByURL(url_with_title.URL);
  }
}
