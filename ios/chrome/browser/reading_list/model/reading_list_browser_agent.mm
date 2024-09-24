// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/features.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/reading_list/model/reading_list_constants.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/l10n/l10n_util.h"

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

  AccountInfo account_info =
      GetAccountInfoFromLastAddedURL(urls.lastObject.URL);

  NSString* snackbar_text = nil;
  MDCSnackbarMessageAction* snackbar_action = nil;
  if (!account_info.IsEmpty()) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_FOR_ACCOUNT);
    std::u16string utf16Text =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "count", (int)urls.count, "email", account_info.email);
    snackbar_text = base::SysUTF16ToNSString(utf16Text);
    static_assert(syncer::IsReadingListAccountStorageEnabled());
    snackbar_action = CreateUndoActionWithReadingListURLs(urls);
  } else {
    snackbar_text =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  }

  MDCSnackbarMessage* message = CreateSnackbarMessage(snackbar_text);
  message.accessibilityLabel = snackbar_text;
  message.action = snackbar_action;

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbar_commands_handler showSnackbarMessage:message];
}

void ReadingListBrowserAgent::BulkAddURLsToReadingListWithViewSnackbar(
    NSArray<NSURL*>* urls) {
  DCHECK([urls count] > 0);
  base::RecordAction(base::UserMetricsAction("IOSReadingListItemsAddedInBulk"));

  ReadingListModel* reading_list_model =
      ReadingListModelFactory::GetInstance()->GetForProfile(
          browser_->GetProfile());
  if (!reading_list_model->loaded()) {
    return;
  }

  // Add reading list items and keep track of successful additions.
  int successfully_added_reading_list_items = 0;
  GURL last_valid_url;

  for (NSURL* ns_url in urls) {
    GURL url = net::GURLWithNSURL(ns_url);

    if (!url.is_valid() || !reading_list_model->IsUrlSupported(url)) {
      continue;
    }

    GURL::Replacements replacements;
    replacements.ClearUsername();
    replacements.ClearPassword();
    replacements.ClearQuery();
    replacements.ClearRef();
    NSString* title =
        base::SysUTF8ToNSString(url.ReplaceComponents(replacements).spec());

    AddURLToReadingListwithTitle(url, title);

    successfully_added_reading_list_items++;
    last_valid_url = url;
  }

  base::UmaHistogramCounts100("IOS.ReadingList.BulkAddURLsCount",
                              successfully_added_reading_list_items);

  NSString* result;
  if (successfully_added_reading_list_items > 0 &&
      !GetAccountInfoFromLastAddedURL(last_valid_url).IsEmpty()) {
    result = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_FOR_ACCOUNT_WITH_COUNT),
            "count", successfully_added_reading_list_items, "email",
            GetAccountInfoFromLastAddedURL(last_valid_url).email));

  } else {
    result = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_NO_ACCOUNT_WITH_COUNT),
            "count", successfully_added_reading_list_items));
  }

  // Create and show snackbar message.
  MDCSnackbarMessageAction* action = CreateViewAction();

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = CreateSnackbarMessage(result);
  message.action = action;

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbar_commands_handler showSnackbarMessage:message];
}

#pragma mark - Private

AccountInfo ReadingListBrowserAgent::GetAccountInfoFromLastAddedURL(
    const GURL& url) {
  ReadingListModel* reading_model =
      ReadingListModelFactory::GetInstance()->GetForProfile(
          browser_->GetProfile());
  CoreAccountId account_id = reading_model->GetAccountWhereEntryIsSavedTo(url);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          browser_->GetProfile()->GetOriginalProfile());
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(account_id);
  return account_info;
}

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

  RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts);
  base::RecordAction(base::UserMetricsAction("MobileReadingListAdd"));
  ReadingListModel* reading_model =
      ReadingListModelFactory::GetInstance()->GetForProfile(
          browser_->GetProfile());
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
      ReadingListModelFactory::GetInstance()->GetForProfile(
          browser_->GetProfile());

  for (URLWithTitle* url_with_title in urls) {
    reading_model->RemoveEntryByURL(url_with_title.URL, FROM_HERE);
  }
}

MDCSnackbarMessageAction* ReadingListBrowserAgent::CreateViewAction() {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  base::WeakPtr<ReadingListBrowserAgent> weak_agent =
      weak_ptr_factory_.GetWeakPtr();
  action.handler = ^{
    base::RecordAction(
        base::UserMetricsAction("IOSReadingListSnackbarViewButtonClicked"));
    ReadingListBrowserAgent* agent = weak_agent.get();
    if (agent) {
      CommandDispatcher* dispatcher = agent->browser_->GetCommandDispatcher();
      id<BrowserCoordinatorCommands> browser_coordinator_commands_handler =
          HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
      [browser_coordinator_commands_handler showReadingList];
    }
  };
  action.title =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_VIEW_ACTION);
  return action;
}
