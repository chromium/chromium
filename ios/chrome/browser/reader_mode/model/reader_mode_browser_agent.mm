// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

#import "base/task/single_thread_task_runner.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_side_swipe_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Delay for the Reader mode chip presentation. This makes the transition
// between the "contextual" Reader mode chip (presented while Reader mode is
// inactive) and this Reader mode chip (presented while Reader mode is active)
// smoother. This is the amount of time it takes for the Reader mode contextual
// chip to contract when it is tapped.
constexpr base::TimeDelta kShowReaderModeChipAnimatedDelay =
    base::Milliseconds(300);

}  // namespace

#pragma mark - Public

ReaderModeBrowserAgent::~ReaderModeBrowserAgent() = default;

void ReaderModeBrowserAgent::SetDelegate(
    id<ReaderModeBrowserAgentDelegate> delegate) {
  delegate_ = delegate;
}

#pragma mark - Private

ReaderModeBrowserAgent::ReaderModeBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  web_state_list_scoped_observation_.Observe(browser->GetWebStateList());
}

void ReaderModeBrowserAgent::ShowReaderModeUI(BOOL animated) {
  crash_keys::SetCurrentlyInReaderMode(true);
  [delegate_ readerModeBrowserAgent:this showContentAnimated:animated];

  __weak id<ReaderModeChipCommands> weak_reader_mode_chip_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(),
                         ReaderModeChipCommands);
  auto show_reader_mode_chip = base::BindOnce(^{
    [weak_reader_mode_chip_handler showReaderModeChip];
  });

  if (animated) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(show_reader_mode_chip),
        kShowReaderModeChipAnimatedDelay);
  } else {
    std::move(show_reader_mode_chip).Run();
  }
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

  if (old_tab_helper) {
    // If there was an active WebState, remove ourself as observer.
    old_tab_helper->RemoveObserver(this);
  }
  if (new_tab_helper) {
    // If there is a new active WebState, start observing it.
    new_tab_helper->AddObserver(this);
  }

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

#pragma mark - ReaderModeTabHelper::Observer

void ReaderModeBrowserAgent::ReaderModeWebStateDidLoadContent(
    ReaderModeTabHelper* tab_helper) {
  FullscreenController* fullscreen_controller =
      FullscreenController::FromBrowser(browser_);
  tab_helper->SetFullscreenController(fullscreen_controller);
  // If Reader mode becomes active in the active WebState, show the Reader mode
  // UI.
  ShowReaderModeUI(/* animated= */ YES);
}

void ReaderModeBrowserAgent::ReaderModeWebStateWillBecomeUnavailable(
    ReaderModeTabHelper* tab_helper,
    ReaderModeDeactivationReason reason) {
  tab_helper->SetFullscreenController(nullptr);
  // If Reader mode becomes inactive in the active WebState, hide the Reader
  // mode UI.
  const bool animated =
      reason == ReaderModeDeactivationReason::kUserDeactivated;
  HideReaderModeUI(animated);
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
    ReaderModeTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
}
