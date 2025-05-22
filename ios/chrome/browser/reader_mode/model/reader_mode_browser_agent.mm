// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"

#pragma mark - Public

ReaderModeBrowserAgent::~ReaderModeBrowserAgent() = default;

void ReaderModeBrowserAgent::SetReaderModeHandler(
    id<ReaderModeCommands> reader_mode_handler) {
  reader_mode_handler_ = reader_mode_handler;
}

#pragma mark - Private

ReaderModeBrowserAgent::ReaderModeBrowserAgent(Browser* browser,
                                               WebStateList* web_state_list)
    : BrowserUserData(browser) {
  web_state_list_scoped_observation_.Observe(web_state_list);
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
      old_tab_helper && old_tab_helper->IsReaderModeWebStateAvailable();
  const bool new_reader_mode_web_state_available =
      new_tab_helper && new_tab_helper->IsReaderModeWebStateAvailable();
  if (!old_reader_mode_web_state_available &&
      new_reader_mode_web_state_available) {
    [reader_mode_handler_ showReaderMode];
  } else if (old_reader_mode_web_state_available &&
             !new_reader_mode_web_state_available) {
    [reader_mode_handler_ hideReaderMode];
  }
}

void ReaderModeBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // If the WebStateList is destroyed, stop observing it.
  web_state_list_scoped_observation_.Reset();
}

#pragma mark - ReaderModeTabHelper::Observer

void ReaderModeBrowserAgent::ReaderModeWebStateDidBecomeAvailable(
    ReaderModeTabHelper* tab_helper) {
  // If Reader mode becomes active in the active WebState, show the Reader mode
  // UI.
  [reader_mode_handler_ showReaderMode];
}

void ReaderModeBrowserAgent::ReaderModeWebStateWillBecomeUnavailable(
    ReaderModeTabHelper* tab_helper) {
  // If Reader mode becomes inactive in the active WebState, hide the Reader
  // mode UI.
  [reader_mode_handler_ hideReaderMode];
}

void ReaderModeBrowserAgent::ReaderModeTabHelperDestroyed(
    ReaderModeTabHelper* tab_helper) {
  tab_helper->RemoveObserver(this);
}
