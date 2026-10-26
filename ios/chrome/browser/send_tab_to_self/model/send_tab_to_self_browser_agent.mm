// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <UIKit/UIKit.h>

#import <algorithm>
#import <string>

#import "base/check.h"
#import "base/containers/span.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
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
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

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
void ShowPostSendSnackbar(id<SnackbarCommands> snackbar_commands,
                          const std::string& target_device_name,
                          NSString* email,
                          send_tab_to_self::SendTabToSelfResult result) {
  if (!snackbar_commands) {
    return;
  }

  NSString* message_text = nil;
  switch (result) {
    case send_tab_to_self::SendTabToSelfResult::kSuccess: {
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      NSString* text =
          l10n_util::GetNSStringF(IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                                  base::UTF8ToUTF16(target_device_name));
      SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
      if (email.length > 0) {
        message.subtitle = email;
      }
      [snackbar_commands showSnackbarMessage:message];
      return;
    }
    case send_tab_to_self::SendTabToSelfResult::kSuccessThrottled: {
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      NSString* text = l10n_util::GetNSStringF(
          IDS_SEND_TAB_TO_SELF_POST_SEND_THROTTLED_TOAST,
          base::UTF8ToUTF16(target_device_name));
      SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:text];
      [snackbar_commands showSnackbarMessage:message];
      return;
    }
    case send_tab_to_self::SendTabToSelfResult::kFailureNoInternetConnection:
    case send_tab_to_self::SendTabToSelfResult::kFailureCommitTimeout: {
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

// Records the scroll position generation outcome and selector length based on
// the generator result, mirroring the metrics recorded on Blink platforms
// (`SendTabToSelfPageHandler::ProcessSelectorGenerationResult`).
//
// Matches Blink metrics for:
// - `kMainFrameChanged`: WebState navigated or changed URL during generation.
// - `kRendererTimeout`: Generator timed out or returned no fragment.
// - `kPageNotScrolled`: Page was not scrolled (scrollY <= 1).
// - `kEmptySelector`: Generation succeeded but produced an empty selector.
// - `kLinkGenerationError`: Polyfill reported invalid selection, ambiguous
//   match, or execution failure.
// - `kSuccess`: Valid text fragment was generated.
//
// Differences from Blink:
// - `kBrowserTimeout`: Not recorded on iOS because selector generation is
//   handled directly via JavaScript execution rather than a separate
//   browser-side timeout timer; timeouts map to `kRendererTimeout`.
// - `kInvalidSelector`: Not recorded on iOS because the polyfill produces a
//   structured `SendTabToSelfTextFragment` rather than an escaped string that
//   requires parsing.
// - Selector length: Recorded as the combined length of text fragment
//   components rather than the raw serialized directive string.
void RecordScrollPositionResult(
    web::WebState* active_web_state,
    const GURL& expected_url,
    const std::optional<SendTabToSelfTextFragment>& fragment) {
  if (!active_web_state ||
      active_web_state->GetLastCommittedURL() != expected_url) {
    send_tab_to_self::RecordScrollPositionGenerationOutcome(
        send_tab_to_self::ScrollPositionGenerationOutcome::kMainFrameChanged);
    return;
  }

  if (!fragment) {
    send_tab_to_self::RecordScrollPositionGenerationOutcome(
        send_tab_to_self::ScrollPositionGenerationOutcome::kRendererTimeout);
    return;
  }

  switch (fragment->status) {
    case TextFragmentGenerationStatus::kSuccess:
      if (fragment->text_start.empty()) {
        send_tab_to_self::RecordScrollPositionGenerationOutcome(
            send_tab_to_self::ScrollPositionGenerationOutcome::kEmptySelector);
        return;
      }
      send_tab_to_self::RecordScrollPositionGenerationOutcome(
          send_tab_to_self::ScrollPositionGenerationOutcome::kSuccess);
      send_tab_to_self::RecordScrollPositionSelectorLength(
          fragment->text_start.length() + fragment->text_end.length() +
          fragment->prefix.length() + fragment->suffix.length());
      return;
    case TextFragmentGenerationStatus::kPageNotScrolled:
      send_tab_to_self::RecordScrollPositionGenerationOutcome(
          send_tab_to_self::ScrollPositionGenerationOutcome::kPageNotScrolled);
      return;
    case TextFragmentGenerationStatus::kTimeout:
      send_tab_to_self::RecordScrollPositionGenerationOutcome(
          send_tab_to_self::ScrollPositionGenerationOutcome::kRendererTimeout);
      return;
    case TextFragmentGenerationStatus::kInvalidSelection:
    case TextFragmentGenerationStatus::kAmbiguous:
    case TextFragmentGenerationStatus::kExecutionFailed:
      send_tab_to_self::RecordScrollPositionGenerationOutcome(
          send_tab_to_self::ScrollPositionGenerationOutcome::
              kLinkGenerationError);
      return;
  }
}

// Records AutoOpenedImmediately and AutoOpenOutcome metrics for `entry` when it
// is auto-opened in the background immediately upon receipt in
// `DisplayNewEntries`.
void RecordAutoOpenMetricsForNewEntry(
    const send_tab_to_self::SendTabToSelfEntry* entry,
    base::Time foreground_session_start) {
  // On iOS, entries shared while the user was away from Chrome can still take
  // the immediate auto-open path in two cases:
  // 1. Chrome is currently in the background
  //    (`foreground_session_start.is_null()`), where the active WebState still
  //    reports `IsVisible() == true` (or Tab Grid auto-open is enabled).
  // 2. The entry was shared before Chrome entered the foreground
  //    (`entry->GetSharedTime() < foreground_session_start`), because iOS
  //    suspends Sync while backgrounded and only downloads the entry shortly
  //    after Chrome returns to the foreground.
  // Record both cases like other platforms do for entries shared while away:
  // `AutoOpenedImmediately` as `false` followed by
  // `AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation`. Note that
  // `GetSharedTime()` uses the sending device's clock, so clock skew between
  // devices can misclassify entries shared close to the foreground transition.
  const bool shared_while_away =
      foreground_session_start.is_null() ||
      entry->GetSharedTime() < foreground_session_start;
  send_tab_to_self::RecordAutoOpenedImmediately(!shared_while_away);
  send_tab_to_self::RecordAutoOpenOutcome(
      shared_while_away ? send_tab_to_self::AutoOpenOutcome::
                              kTabsOpenedInBackgroundUponActivation
                        : send_tab_to_self::AutoOpenOutcome::
                              kTabsOpenedImmediatelyInBackground);
}

}  // namespace

SendTabToSelfBrowserAgent::SendTabToSelfBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      model_(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile())
              ->GetSendTabToSelfModel()),
      foreground_session_start_(base::Time::Now()) {
  browser_observation_.Observe(browser_);
  model_observation_.Observe(model_.get());
  UrlLoadingNotifierBrowserAgent* loading_notifier =
      UrlLoadingNotifierBrowserAgent::FromBrowser(browser_);
  if (loading_notifier) {
    url_loading_observation_.Observe(loading_notifier);
  }
  web_state_list_observation_.Observe(browser_->GetWebStateList());
  RegisterAppLifecycleObservers();
  if (web::WebState* web_state =
          browser_->GetWebStateList()->GetActiveWebState()) {
    web_state_observation_.Observe(web_state);
  }
  if (model_->IsReady()) {
    OnModelReady();
  }
}

SendTabToSelfBrowserAgent::~SendTabToSelfBrowserAgent() = default;

void SendTabToSelfBrowserAgent::BrowserDestroyed(Browser* browser) {
  // Invalidate weak pointers and unregister NSNotificationCenter observers
  // before tearing down browser state.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (application_backgrounding_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_backgrounding_observer_];
    application_backgrounding_observer_ = nil;
  }
  if (application_foregrounding_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_foregrounding_observer_];
    application_foregrounding_observer_ = nil;
  }
  web_state_list_observation_.Reset();
  web_state_observation_.Reset();
  url_loading_observation_.Reset();
  model_observation_.Reset();
  browser_observation_.Reset();
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
  CheckAndOpenPendingEntriesIfBrowserVisible();
}

#pragma mark - ReceivingUiHandler

void SendTabToSelfBrowserAgent::DisplayNewEntries(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
  if (new_entries.empty()) {
    return;
  }

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
      RecordAutoOpenMetricsForNewEntry(entry, foreground_session_start_);
    }
    // Only display the infobar banner if the active WebState is currently
    // visible (i.e., user is not in the Tab Grid screen or a Settings page).
    if (web_state->IsVisible()) {
      DisplayInfoBar(web_state, GetMostRecentlySharedEntry(new_entries),
                     new_entries.size());
    }
  } else {
    send_tab_to_self::RecordAutoOpenedImmediately(false, new_entries.size());
  }
}

void SendTabToSelfBrowserAgent::DismissEntries(
    base::span<const std::string> /*guids*/) {
  // The tabs should have already been auto-opened. Leave them be.
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

  web_state_observation_.Reset();
  web_state_observation_.Observe(new_active);
  CheckAndOpenPendingEntriesIfBrowserVisible();
}

void SendTabToSelfBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_list_observation_.Reset();
}

#pragma mark - WebStateObserver

void SendTabToSelfBrowserAgent::WasShown(web::WebState* web_state) {
  // Auto-open pending entries as the web state is now visible.
  CheckAndOpenPendingEntriesIfBrowserVisible();
}

void SendTabToSelfBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
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

void SendTabToSelfBrowserAgent::RegisterAppLifecycleObservers() {
  // Observe app background and foreground transitions to track when the
  // current foreground session started for AutoOpenedImmediately and
  // AutoOpenOutcome metrics.
  // UIApplicationDidEnterBackgroundNotification and
  // UIApplicationWillEnterForegroundNotification are used instead of
  // active/inactive notifications so that transient inactive states (such as
  // system alerts, notification banners, or Control Center) do not reset
  // `foreground_session_start_`.
  base::WeakPtr<SendTabToSelfBrowserAgent> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  application_backgrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (weak_ptr) {
                  weak_ptr->foreground_session_start_ = base::Time();
                }
              }];
  application_foregrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (weak_ptr) {
                  weak_ptr->foreground_session_start_ = base::Time::Now();
                }
              }];
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

void SendTabToSelfBrowserAgent::CheckAndOpenPendingEntriesIfBrowserVisible() {
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
  if (web_state) {
    page_context = send_tab_to_self::ExtractFormFieldsFromWebState(web_state);
  }

  std::optional<send_tab_to_self::ScrollPositionGenerationOutcome>
      failure_outcome;
  if (!web_state) {
    failure_outcome = send_tab_to_self::ScrollPositionGenerationOutcome::
        kMainFrameUnavailable;
  } else if (web_state->GetLastCommittedURL() != url) {
    failure_outcome =
        send_tab_to_self::ScrollPositionGenerationOutcome::kMainFrameChanged;
  } else if (web_state->IsLoading()) {
    failure_outcome = send_tab_to_self::ScrollPositionGenerationOutcome::
        kMainFrameUnavailable;
  }

  if (failure_outcome) {
    send_tab_to_self::RecordScrollPositionGenerationOutcome(*failure_outcome);
    HandleTextFragmentGenerated(url, title, target_guid, target_device_name,
                                entry_point, std::move(page_context),
                                std::move(send_result_callback),
                                /*start_time=*/std::nullopt, std::nullopt);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  SendTabToSelfTextFragmentSelectorGenerator::GetInstance()->GetTextFragment(
      web_state,
      base::BindOnce(&SendTabToSelfBrowserAgent::HandleTextFragmentGenerated,
                     weak_ptr_factory_.GetWeakPtr(), url, title, target_guid,
                     target_device_name, entry_point, std::move(page_context),
                     std::move(send_result_callback), start_time));
}

void SendTabToSelfBrowserAgent::HandleTextFragmentGenerated(
    const GURL& url,
    const std::string& title,
    const std::string& target_guid,
    const std::string& target_device_name,
    send_tab_to_self::ShareEntryPoint entry_point,
    send_tab_to_self::PageContext page_context,
    SendResultCallback send_result_callback,
    std::optional<base::TimeTicks> start_time,
    std::optional<SendTabToSelfTextFragment> fragment) {
  if (!browser_) {
    return;
  }

  if (start_time.has_value()) {
    send_tab_to_self::RecordScrollPositionGenerationTime(
        base::TimeTicks::Now() - *start_time);
    web::WebState* active_web_state =
        browser_->GetWebStateList()->GetActiveWebState();
    RecordScrollPositionResult(active_web_state, url, fragment);
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

  if (fragment && fragment->status == TextFragmentGenerationStatus::kSuccess &&
      !fragment->text_start.empty()) {
    page_context.scroll_position.text_fragment =
        send_tab_to_self::TextFragmentData(fragment->text_start,
                                           fragment->text_end, fragment->prefix,
                                           fragment->suffix);
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

void SendTabToSelfBrowserAgent::HandleTextFragmentGeneratedForTesting(
    const GURL& url,
    const std::string& title,
    const std::string& target_guid,
    const std::string& target_device_name,
    send_tab_to_self::ShareEntryPoint entry_point,
    send_tab_to_self::PageContext page_context,
    SendResultCallback send_result_callback,
    std::optional<base::TimeTicks> start_time,
    std::optional<SendTabToSelfTextFragment> text_fragment) {
  HandleTextFragmentGenerated(url, title, target_guid, target_device_name,
                              entry_point, std::move(page_context),
                              std::move(send_result_callback), start_time,
                              std::move(text_fragment));
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
