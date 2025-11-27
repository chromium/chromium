// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

#import "base/task/single_thread_task_runner.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

#pragma mark - Public

ReaderModeBrowserAgent::~ReaderModeBrowserAgent() = default;

void ReaderModeBrowserAgent::SetDelegate(
    id<ReaderModeBrowserAgentDelegate> delegate) {
  delegate_ = delegate;
}

#pragma mark - Private

ReaderModeBrowserAgent::ReaderModeBrowserAgent(Browser* browser)
    : BrowserUserData(browser), bridge_(browser) {
  web_state_list_scoped_observation_.Observe(browser->GetWebStateList());
}

web::WebState* ReaderModeBrowserAgent::GetActiveWebState() {
  return browser_->GetWebStateList()->GetActiveWebState();
}

void ReaderModeBrowserAgent::AttachWebState(web::WebState* web_state) {
  if (web_state->IsRealized()) {
    AttachReaderModeTabHelper(ReaderModeTabHelper::FromWebState(web_state));
  } else {
    web_state_scoped_observation_.AddObservation(web_state);
  }
}

void ReaderModeBrowserAgent::DetachWebState(web::WebState* web_state) {
  if (web_state->IsRealized()) {
    DetachReaderModeTabHelper(ReaderModeTabHelper::FromWebState(web_state));
  } else {
    web_state_scoped_observation_.RemoveObservation(web_state);
  }
}

ReaderModeTabHelper* ReaderModeBrowserAgent::GetActiveReaderModeTabHelper() {
  return ReaderModeTabHelper::FromWebState(GetActiveWebState());
}

void ReaderModeBrowserAgent::AttachReaderModeTabHelper(
    ReaderModeTabHelper* tab_helper) {
  reader_mode_tab_helper_scoped_observation_.AddObservation(tab_helper);
  if (tab_helper->GetReaderModeWebState()) {
    bridge_.ReaderModeWebStateDidLoadContent(
        tab_helper->GetReaderModeWebState());
  }
}

void ReaderModeBrowserAgent::DetachReaderModeTabHelper(
    ReaderModeTabHelper* tab_helper) {
  if (tab_helper->GetReaderModeWebState()) {
    bridge_.ReaderModeWebStateWillBecomeUnavailable(
        tab_helper->GetReaderModeWebState());
  }
  reader_mode_tab_helper_scoped_observation_.RemoveObservation(tab_helper);
}

void ReaderModeBrowserAgent::ShowReaderModeUI(BOOL animated) {
  // Notify that the Reader Mode UI has been started.
  feature_engagement::TrackerFactory::GetForProfile(browser_->GetProfile())
      ->NotifyEvent(feature_engagement::events::kIOSReaderModeUsed);
  crash_keys::SetCurrentlyInReaderMode(true);
  [delegate_ readerModeBrowserAgent:this showContentAnimated:animated];

  id<ReaderModeChipCommands> reader_mode_chip_handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), ReaderModeChipCommands);
  [reader_mode_chip_handler showReaderModeChip];
  id<ContextualPanelEntrypointCommands> contextual_panel_entrypoint_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(),
                         ContextualPanelEntrypointCommands);
  [contextual_panel_entrypoint_handler
      cancelContextualPanelEntrypointLoudMoment];

  UpdateHandlersOnActiveWebState();
}

void ReaderModeBrowserAgent::HideReaderModeUI(BOOL animated) {
  crash_keys::SetCurrentlyInReaderMode(false);
  id<ReaderModeChipCommands> reader_mode_chip_handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), ReaderModeChipCommands);
  [reader_mode_chip_handler hideReaderModeChip];
  [delegate_ readerModeBrowserAgent:this hideContentAnimated:animated];

  UpdateHandlersOnActiveWebState();
}

void ReaderModeBrowserAgent::UpdateHandlersOnActiveWebState() {
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<PageSideSwipeCommands> pageSideSwipeHandler =
      HandlerForProtocol(dispatcher, PageSideSwipeCommands);
  [pageSideSwipeHandler updateEdgeSwipePrecedenceForActiveWebState];
}

#pragma mark - WebStateListObserver

void ReaderModeBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Change in active WebState is handled after switch.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      DetachWebState(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      DetachWebState(replace_change.replaced_web_state());
      AttachWebState(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      AttachWebState(insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      break;
  }

  if (!status.active_web_state_change()) {
    // If there is no change in active WebState, do nothing.
    return;
  }

  ReaderModeTabHelper* const old_tab_helper =
      status.old_active_web_state
          ? ReaderModeTabHelper::FromWebState(status.old_active_web_state)
          : nullptr;
  ReaderModeTabHelper* const new_tab_helper =
      status.new_active_web_state
          ? ReaderModeTabHelper::FromWebState(status.new_active_web_state)
          : nullptr;

  // Show or hide the Reader mode UI if necessary.
  const bool old_reader_mode_web_state_available =
      old_tab_helper && (old_tab_helper->GetReaderModeWebState() != nullptr);
  const bool new_reader_mode_web_state_available =
      new_tab_helper && (new_tab_helper->GetReaderModeWebState() != nullptr);
  if (!old_reader_mode_web_state_available &&
      new_reader_mode_web_state_available) {
    ShowReaderModeUI(/* animated= */ NO);
  } else if (old_reader_mode_web_state_available &&
             !new_reader_mode_web_state_available) {
    HideReaderModeUI(/* animated= */ NO);
  }
}

void ReaderModeBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // If the WebStateList is destroyed, stop observing it.
  web_state_list_scoped_observation_.Reset();
}

#pragma mark - web::WebStateObserver

void ReaderModeBrowserAgent::WebStateRealized(web::WebState* web_state) {
  web_state_scoped_observation_.RemoveObservation(web_state);
  AttachReaderModeTabHelper(ReaderModeTabHelper::FromWebState(web_state));
}

void ReaderModeBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  NOTREACHED();
}

#pragma mark - ReaderModeTabHelper::Observer

void ReaderModeBrowserAgent::ReaderModeWebStateDidLoadContent(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
  bridge_.ReaderModeWebStateDidLoadContent(web_state);

  if (tab_helper == GetActiveReaderModeTabHelper()) {
    // If Reader mode becomes active in the active WebState, show the Reader
    // mode UI.
    ShowReaderModeUI(/* animated= */ YES);
  }
}

void ReaderModeBrowserAgent::ReaderModeWebStateWillBecomeUnavailable(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state,
    ReaderModeDeactivationReason reason) {
  if (tab_helper == GetActiveReaderModeTabHelper()) {
    // If Reader mode becomes inactive in the active WebState, hide the Reader
    // mode UI.
    const bool animated =
        reason == ReaderModeDeactivationReason::kUserDeactivated;
    HideReaderModeUI(animated);
  }

  bridge_.ReaderModeWebStateWillBecomeUnavailable(web_state);
}

void ReaderModeBrowserAgent::ReaderModeDistillationFailed(
    ReaderModeTabHelper* tab_helper) {
  // Show distillation failure snackbar.
  id<SnackbarCommands> snackbar_handler =
      static_cast<id<SnackbarCommands>>(browser_->GetCommandDispatcher());
  [snackbar_handler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_READER_MODE_SNACKBAR_FAILURE_MESSAGE)
                   buttonText:l10n_util::GetNSString(IDS_DONE)
                messageAction:nil
             completionAction:nil];
}

void ReaderModeBrowserAgent::ReaderModeTabHelperDestroyed(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
  tab_helper->RemoveObserver(this);
  bridge_.ReaderModeTabHelperDestroyed(web_state);
}
