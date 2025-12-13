// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/profile_incognito_session_tracker.h"

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

ProfileIncognitoSessionTracker::ProfileIncognitoSessionTracker(
    BrowserList* browser_list,
    Callback callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  browser_list_observation_.Observe(browser_list);

  // Observe all pre-existing off-the-record Browsers.
  const auto kIncognitoBrowserType = BrowserList::BrowserType::kIncognito;
  for (Browser* browser : browser_list->BrowsersOfType(kIncognitoBrowserType)) {
    web_state_list_observations_.AddObservation(browser->GetWebStateList());
  }

  // Check whether any of the Browsers has any open off-the-record tabs.
  OnWebStateListChanged();
}

ProfileIncognitoSessionTracker::~ProfileIncognitoSessionTracker() = default;

void ProfileIncognitoSessionTracker::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  // Ignore non-incognito Browsers.
  if (browser->type() != Browser::Type::kIncognito) {
    return;
  }

  WebStateList* const web_state_list = browser->GetWebStateList();
  web_state_list_observations_.AddObservation(web_state_list);

  // If the WebStateList was not empty, then it may be necessary to
  // notify the callback.
  if (!web_state_list->empty()) {
    OnWebStateListChanged();
  }
}

void ProfileIncognitoSessionTracker::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  // Ignore non-incognito Browsers.
  if (browser->type() != Browser::Type::kIncognito) {
    return;
  }

  WebStateList* const web_state_list = browser->GetWebStateList();
  web_state_list_observations_.RemoveObservation(web_state_list);

  // If the WebStateList was not empty, then it may be necessary to
  // notify the callback.
  if (!web_state_list->empty()) {
    OnWebStateListChanged();
  }
}

void ProfileIncognitoSessionTracker::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  // Ignore changes during batch operations.
  if (web_state_list->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    // None of those events can change the number of off-the-record tabs,
    // ignore them.
    case WebStateListChange::Type::kStatusOnly:
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kReplace:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      return;

    // Those events either increment or decrement the number of open
    // off-the-record tabs, so update the state.
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kInsert:
      OnWebStateListChanged();
      return;
  }
}

void ProfileIncognitoSessionTracker::OnWebStateListChanged() {
  const bool has_incognito_tabs = std::ranges::any_of(
      web_state_list_observations_.sources(),
      [](WebStateList* web_state_list) { return !web_state_list->empty(); });

  if (has_incognito_tabs_ != has_incognito_tabs) {
    has_incognito_tabs_ = has_incognito_tabs;
    callback_.Run(has_incognito_tabs_);
  }
}

void ProfileIncognitoSessionTracker::BatchOperationEnded(
    WebStateList* web_state_list) {
  // Anything can change during a batch operation. Update the state.
  OnWebStateListChanged();
}

void ProfileIncognitoSessionTracker::OnBrowserListShutdown(
    BrowserList* browser_list) {
  browser_list_observation_.Reset();
}
