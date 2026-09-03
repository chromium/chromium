// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <string>

#import "base/check.h"
#import "base/check_op.h"
#import "base/containers/span.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/not_fatal_until.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Helper function to remove infobars corresponding to the removed GUIDs from
// the given WebState.
void RemoveInfoBarsForGUIDs(web::WebState* web_state,
                            base::span<const std::string> guids) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!infobar_manager) {
    return;
  }

  std::vector<infobars::InfoBar*> infobars_to_remove;
  for (infobars::InfoBar* infobar : infobar_manager->infobars()) {
    if (infobar->GetIdentifier() !=
        infobars::InfoBarDelegate::SEND_TAB_TO_SELF_INFOBAR_DELEGATE) {
      continue;
    }

    auto* delegate =
        static_cast<send_tab_to_self::IOSSendTabToSelfInfoBarDelegate*>(
            infobar->delegate());
    if (std::ranges::contains(guids, delegate->GetGUID())) {
      infobars_to_remove.push_back(infobar);
    }
  }

  for (infobars::InfoBar* infobar : infobars_to_remove) {
    infobar_manager->RemoveInfoBar(infobar);
  }
}

// Returns the most recently shared SendTabToSelfEntry from a span of entries.
const send_tab_to_self::SendTabToSelfEntry* GetMostRecentlySharedEntry(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> entries) {
  CHECK(!entries.empty());
  auto max_it = std::ranges::max_element(
      entries, [](const send_tab_to_self::SendTabToSelfEntry* first_entry,
                  const send_tab_to_self::SendTabToSelfEntry* second_entry) {
        return first_entry->GetSharedTime() < second_entry->GetSharedTime();
      });
  return *max_it;
}

// Displays the appropriate post-send snackbar or toast based on `result`.
void ShowPostSendSnackbar(
    id<SnackbarCommands> snackbar_commands,
    const std::string& target_device_name,
    NSString* email,
    send_tab_to_self::SendTabToSelfResult result) {
  if (!snackbar_commands) {
    return;
  }

  bool post_send_toast_enabled = base::FeatureList::IsEnabled(
      send_tab_to_self::kSendTabToSelfPostSendToast);

  NSString* message_text = nil;
  switch (result) {
    case send_tab_to_self::SendTabToSelfResult::kSuccess: {
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      NSString* text = nil;
      if (!post_send_toast_enabled) {
        text = l10n_util::GetNSStringF(
            IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
            base::UTF8ToUTF16(target_device_name));
      } else {
        text = l10n_util::GetNSStringF(
            IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
            base::UTF8ToUTF16(target_device_name));
      }
      SnackbarMessage* message =
          [[SnackbarMessage alloc] initWithTitle:text];
      if (post_send_toast_enabled && email.length > 0) {
        message.subtitle = email;
      }
      [snackbar_commands showSnackbarMessage:message];
      return;
    }
    case send_tab_to_self::SendTabToSelfResult::kSuccessThrottled: {
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      NSString* text = nil;
      if (!post_send_toast_enabled) {
        text = l10n_util::GetNSStringF(
            IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
            base::UTF8ToUTF16(target_device_name));
      } else {
        text = l10n_util::GetNSStringF(
            IDS_SEND_TAB_TO_SELF_POST_SEND_THROTTLED_TOAST,
            base::UTF8ToUTF16(target_device_name));
      }
      SnackbarMessage* message =
          [[SnackbarMessage alloc] initWithTitle:text];
      [snackbar_commands showSnackbarMessage:message];
      return;
    }
    case send_tab_to_self::SendTabToSelfResult::kFailureNoInternetConnection:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitTimeout: {
      if (!post_send_toast_enabled) {
        return;
      }
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeError);
      message_text = l10n_util::GetNSString(
          IDS_SEND_TAB_TO_SELF_POST_SEND_NO_INTERNET_TOAST);
      break;
    }
    case send_tab_to_self::SendTabToSelfResult::kFailureNotTrackingMetadata:
    case send_tab_to_self::SendTabToSelfResult::kFailureInvalidUrl:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitAttemptFailed:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitAttemptError:
    case send_tab_to_self::SendTabToSelfResult::kFailureSyncDisabled:
    case send_tab_to_self::SendTabToSelfResult::kFailureEntryRemoved: {
      if (!post_send_toast_enabled) {
        return;
      }
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeError);
      message_text =
          l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_POST_SEND_FAILURE_TOAST);
      break;
    }
  }

  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:message_text];
  [snackbar_commands showSnackbarMessage:message];
}

}  // namespace

SendTabToSelfBrowserAgent::SendTabToSelfBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      model_(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile())
              ->GetSendTabToSelfModel()) {
  browser_observation_.Observe(browser_);
  model_observation_.Observe(model_.get());
  UrlLoadingNotifierBrowserAgent* loading_notifier =
      UrlLoadingNotifierBrowserAgent::FromBrowser(browser_);
  if (loading_notifier) {
    url_loading_observation_.Observe(loading_notifier);
  }
  web_state_list_observation_.Observe(browser_->GetWebStateList());
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    if (web::WebState* web_state =
            browser_->GetWebStateList()->GetActiveWebState()) {
      web_state_observation_.Observe(web_state);
    }
    if (model_->IsReady()) {
      OnModelReady();
    }
  }
}

SendTabToSelfBrowserAgent::~SendTabToSelfBrowserAgent() = default;

void SendTabToSelfBrowserAgent::BrowserDestroyed(Browser* browser) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  web_state_list_observation_.Reset();
  url_loading_observation_.Reset();
  model_observation_.Reset();
  browser_observation_.Reset();
  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::OnEntriesAddedRemotely(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
  DisplayNewEntries(new_entries);
}

void SendTabToSelfBrowserAgent::OnEntriesRemovedRemotely(
    base::span<const std::string> guids) {
  DismissEntries(guids);
}

void SendTabToSelfBrowserAgent::OnModelReady() {
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    CheckAndOpenPendingEntriesIfBrowserVisible();
  }
}

#pragma mark - ReceivingUiHandler

void SendTabToSelfBrowserAgent::DisplayNewEntries(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
  if (new_entries.empty()) {
    return;
  }

  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
    // If there is an active WebState, auto-open entries in the background
    // immediately so they appear in the Tab Grid with their activity label even
    // if the user is currently on the tab switcher.
    const bool should_auto_open =
        web_state &&
        (web_state->IsVisible() ||
         base::FeatureList::IsEnabled(
             send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid));
    if (should_auto_open) {
      for (const send_tab_to_self::SendTabToSelfEntry* entry : new_entries) {
        OpenEntryInBackgroundTab(entry);
        send_tab_to_self::RecordAutoOpenOutcome(
            send_tab_to_self::AutoOpenOutcome::
                kTabsOpenedImmediatelyInBackground);
      }
      // Only display the infobar banner if the active WebState is currently
      // visible (i.e., user is not in the Tab Grid screen or a Settings page).
      if (web_state->IsVisible()) {
        DisplayInfoBar(web_state, GetMostRecentlySharedEntry(new_entries),
                       new_entries.size());
      }
    } else {
      for (size_t ii = 0; ii < new_entries.size(); ++ii) {
        send_tab_to_self::RecordAutoOpenOutcome(
            send_tab_to_self::AutoOpenOutcome::kUnopenedImmediately);
      }
    }
    return;
  }

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state || !web_state->IsVisible()) {
    // If the active WebState is not visible it means the user is in the
    // Tab Grid screen or a Settings page. Register as an observer of the
    // active WebState and WebStateList in order to be notified if the WebState
    // becomes visible again, or if the user changes tab or creates a new tab.
    if (web_state) {
      pending_web_state_ = web_state;
      web_state_observation_.Reset();
      web_state_observation_.Observe(pending_web_state_.get());
    }

    // Pick the most recently shared entry since only one infobar can be shown
    // at a time.
    const send_tab_to_self::SendTabToSelfEntry* entry =
        GetMostRecentlySharedEntry(new_entries);
    pending_entry_guid_ =
        entry ? std::make_optional(entry->GetGUID()) : std::nullopt;

    return;
  }

  // Since only one infobar can be shown at a time, pick the most recently
  // shared entry.
  DisplayInfoBar(web_state, GetMostRecentlySharedEntry(new_entries),
                 new_entries.size());
}

void SendTabToSelfBrowserAgent::DismissEntries(
    base::span<const std::string> guids) {
  if (guids.empty()) {
    return;
  }

  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    // The tabs should have already been auto-opened. Leave them be.
    return;
  }

  if (pending_entry_guid_ &&
      std::ranges::contains(guids, *pending_entry_guid_)) {
    CleanUpObserversAndVariables();
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int i = 0; i < web_state_list->count(); ++i) {
    RemoveInfoBarsForGUIDs(web_state_list->GetWebStateAt(i), guids);
  }
}

#pragma mark - WebStateListObserver

void SendTabToSelfBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (!detach_change.is_user_action() && !detach_change.is_tabs_cleanup()) {
    return;
  }

  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    // If the tab is being closed explicitly by the user (and not due to browser
    // shutdown, tab strip destruction, or tab dragging between windows), log
    // the abandonment metric.
    web::WebState* web_state = detach_change.detached_web_state();
    SendTabToSelfTabCardLabelData* label_data =
        SendTabToSelfTabCardLabelData::FromWebState(web_state);
    if (label_data) {
      label_data->WebStateClosedByUser(web_state);
    }
  }
}

void SendTabToSelfBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  web::WebState* new_active = status.new_active_web_state;
  if (!new_active) {
    return;
  }

  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    web_state_observation_.Reset();
    web_state_observation_.Observe(new_active);
    CheckAndOpenPendingEntriesIfBrowserVisible();
    return;
  }

  if (!pending_entry_guid_) {
    return;
  }

  const send_tab_to_self::SendTabToSelfEntry* entry =
      model_->GetEntryByGUID(*pending_entry_guid_);
  if (entry) {
    DisplayInfoBar(new_active, entry, /*opened_tab_count=*/1);
  }
  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_list_observation_.Reset();
}

#pragma mark - WebStateObserver

void SendTabToSelfBrowserAgent::WasShown(web::WebState* web_state) {
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    // Auto-open pending entries as the web state is now visible.
    CheckAndOpenPendingEntriesIfBrowserVisible();
    return;
  }

  CHECK(pending_entry_guid_.has_value(), base::NotFatalUntil::M158);
  CHECK(pending_web_state_, base::NotFatalUntil::M158);

  const send_tab_to_self::SendTabToSelfEntry* entry =
      model_->GetEntryByGUID(*pending_entry_guid_);
  if (entry) {
    DisplayInfoBar(pending_web_state_, entry, /*opened_tab_count=*/1);
  }

  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    web_state_observation_.Reset();
    return;
  }

  CHECK(pending_web_state_, base::NotFatalUntil::M158);
  CHECK_EQ(pending_web_state_, web_state, base::NotFatalUntil::M158);

  web_state_observation_.Reset();
  pending_web_state_ = nullptr;
}

void SendTabToSelfBrowserAgent::DisplayInfoBar(
    web::WebState* web_state,
    const send_tab_to_self::SendTabToSelfEntry* entry,
    size_t opened_tab_count) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  if (!infobar_manager) {
    return;
  }

  send_tab_to_self::RecordNotificationStatus(
      send_tab_to_self::NotificationStatus::kShown);

  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      send_tab_to_self::IOSSendTabToSelfInfoBarDelegate::Create(
          entry, opened_tab_count, model_,
          HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands),
          browser_->GetWebStateList())));
}

void SendTabToSelfBrowserAgent::CleanUpObserversAndVariables() {
  pending_entry_guid_.reset();

  web_state_observation_.Reset();
  pending_web_state_ = nullptr;
}

void SendTabToSelfBrowserAgent::TabWillLoadUrl(
    const UrlLoadParams& params,
    base::WeakPtr<web::WebState> web_state) {
  if (!web_state) {
    return;
  }
  // Always remove old data to ensure we don't use stale GUIDs.
  SendTabToSelfLoadNavigationUserData::RemoveFromWebState(web_state.get());

  if (params.is_from_send_tab_to_self()) {
    SendTabToSelfLoadNavigationUserData::CreateForWebState(
        web_state.get(), params.send_tab_to_self_entry_guid);

    if (base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfAutoOpen)) {
      // Attach a tab card label to the web state for the tab switcher UI.
      const send_tab_to_self::SendTabToSelfEntry* entry =
          model_->GetEntryByGUID(params.send_tab_to_self_entry_guid);
      if (entry
          // Only attach to tabs opened in the background. This is to avoid the
          // case where tabs are opened via the system-level notification.
          && params.in_background()) {
        SendTabToSelfTabCardLabelData::CreateForWebState(
            web_state.get(), entry->GetGUID(), entry->GetDeviceName());
      }
    }
  }
}

void SendTabToSelfBrowserAgent::CheckAndOpenPendingEntriesIfBrowserVisible() {
  CHECK(base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen));

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state) {
    return;
  }

  const bool can_open =
      web_state->IsVisible() ||
      base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid);
  if (!can_open) {
    return;
  }

  std::vector<const send_tab_to_self::SendTabToSelfEntry*> pending_entries =
      model_->GetUnopenedEntriesTargetedToLocalDevice();
  if (pending_entries.empty()) {
    return;
  }

  for (const send_tab_to_self::SendTabToSelfEntry* entry : pending_entries) {
    OpenEntryInBackgroundTab(entry);
    send_tab_to_self::RecordAutoOpenOutcome(
        send_tab_to_self::AutoOpenOutcome::
            kTabsOpenedInBackgroundUponActivation);
  }
  if (web_state->IsVisible()) {
    // Show an infobar for the most recently shared entry among the pending
    // entries.
    DisplayInfoBar(web_state, GetMostRecentlySharedEntry(pending_entries),
                   pending_entries.size());
  }
}

void SendTabToSelfBrowserAgent::SendTabToTargetDevice(
    const GURL& url,
    const std::string& title,
    const std::string& target_guid,
    const std::string& target_device_name,
    send_tab_to_self::ShareEntryPoint entry_point,
    SendResultCallback send_result_callback) {
  if (!browser_) {
    return;
  }

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  send_tab_to_self::PageContext page_context;
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateFormFields) &&
      web_state) {
    page_context = send_tab_to_self::ExtractFormFieldsFromWebState(web_state);
  }

  if (!web_state || web_state->IsLoading() ||
      web_state->GetLastCommittedURL() != url ||
      !base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateScrollPosition)) {
    HandleTextFragmentGenerated(url, title, target_guid, target_device_name,
                                entry_point, std::move(page_context),
                                std::move(send_result_callback), std::nullopt);
    return;
  }

  SendTabToSelfTextFragmentSelectorGenerator::GetInstance()->GetTextFragment(
      web_state,
      base::BindOnce(&SendTabToSelfBrowserAgent::HandleTextFragmentGenerated,
                     weak_ptr_factory_.GetWeakPtr(), url, title, target_guid,
                     target_device_name, entry_point, std::move(page_context),
                     std::move(send_result_callback)));
}

void SendTabToSelfBrowserAgent::HandleTextFragmentGenerated(
    const GURL& url,
    const std::string& title,
    const std::string& target_guid,
    const std::string& target_device_name,
    send_tab_to_self::ShareEntryPoint entry_point,
    send_tab_to_self::PageContext page_context,
    SendResultCallback send_result_callback,
    std::optional<SendTabToSelfTextFragment> fragment) {
  if (!browser_) {
    return;
  }

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile());
  if (!service || !service->GetSendTabToSelfModel()) {
    return;
  }

  id<SnackbarCommands> snackbar_commands = nil;
  if (browser_->GetCommandDispatcher() &&
      [browser_->GetCommandDispatcher()
          dispatchingForProtocol:@protocol(SnackbarCommands)]) {
    snackbar_commands =
        HandlerForProtocol(browser_->GetCommandDispatcher(), SnackbarCommands);
  }

  if (fragment &&
      fragment->status == TextFragmentGenerationStatus::kSuccess &&
      !fragment->text_start.empty()) {
    page_context.scroll_position.text_fragment =
        send_tab_to_self::TextFragmentData(
            fragment->text_start, fragment->text_end,
            fragment->prefix, fragment->suffix);
  }

  service->GetSendTabToSelfModel()->SendEntry(
      url, title, target_guid, page_context,
      send_tab_to_self::NavigationHistory(),
      base::BindOnce(&SendTabToSelfBrowserAgent::HandleEntrySent,
                     weak_ptr_factory_.GetWeakPtr(), snackbar_commands,
                     target_device_name, std::move(send_result_callback)),
      entry_point);
}

void SendTabToSelfBrowserAgent::HandleEntrySent(
    id<SnackbarCommands> snackbar_commands,
    const std::string& target_device_name,
    SendResultCallback send_result_callback,
    send_tab_to_self::SendTabToSelfResult result) {
  if (!snackbar_commands) {
    if (send_result_callback) {
      std::move(send_result_callback).Run(result);
    }
    return;
  }

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  id<SystemIdentity> account =
      auth_service ? auth_service->GetPrimaryIdentity() : nil;
  NSString* email = account ? account.userEmail : nil;

  ShowPostSendSnackbar(snackbar_commands, target_device_name, email, result);

  if (send_result_callback) {
    std::move(send_result_callback).Run(result);
  }
}

void SendTabToSelfBrowserAgent::HandleEntrySentForTest(
    id<SnackbarCommands> snackbar_commands,
    const std::string& target_device_name,
    send_tab_to_self::SendTabToSelfResult result) {
  HandleEntrySent(snackbar_commands, target_device_name, base::NullCallback(),
                  result);
}

void SendTabToSelfBrowserAgent::OpenEntryInBackgroundTab(
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  CHECK(entry);
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfSupportAutoOpenInTabGrid)) {
    UrlLoadParams params = UrlLoadParams::InNewTab(entry->GetURL());
    params.SetInBackground(YES);
    params.append_to = OpenPosition::kCurrentTab;
    params.send_tab_to_self_entry_guid = entry->GetGUID();
    UrlLoadingBrowserAgent::FromBrowser(browser_)->Load(params);
  } else {
    id<SceneCommands> scene_handler =
        HandlerForProtocol(browser_->GetCommandDispatcher(), SceneCommands);
    [scene_handler
        openURLInNewTab:send_tab_to_self::CreateOpenNewBackgroundTabCommand(
                            entry)];
  }

  model_->MarkEntryOpened(entry->GetGUID());
}
