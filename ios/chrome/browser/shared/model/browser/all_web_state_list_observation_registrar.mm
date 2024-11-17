// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"

namespace {

using ObservationMode = AllWebStateListObservationRegistrar::Mode;

// Returns events for Browser of `type` should be propagated
// or ignored, according to `mode`.
bool ShouldPropagateEvent(Browser::Type type, ObservationMode mode) {
  switch (type) {
    case Browser::Type::kIncognito:
      switch (mode) {
        case ObservationMode::INCOGNITO:
        case ObservationMode::ALL:
          return true;

        case ObservationMode::REGULAR:
          return false;
      }

    case Browser::Type::kRegular:
    case Browser::Type::kInactive:
      switch (mode) {
        case ObservationMode::REGULAR:
        case ObservationMode::ALL:
          return true;

        case ObservationMode::INCOGNITO:
          return false;
      }

    case Browser::Type::kTemporary:
      // Never propagate temporary Browser events.
      return false;
  }
}

// Returns the browser types associated with `mode`.
BrowserList::BrowserType BrowserTypesForMode(ObservationMode mode) {
  switch (mode) {
    case ObservationMode::REGULAR:
      return BrowserList::BrowserType::kRegularAndInactive;

    case ObservationMode::INCOGNITO:
      return BrowserList::BrowserType::kIncognito;

    case ObservationMode::ALL:
      return BrowserList::BrowserType::kAll;
  }
}

}  // namespace

AllWebStateListObservationRegistrar::AllWebStateListObservationRegistrar(
    BrowserList* browser_list,
    std::unique_ptr<WebStateListObserver> web_state_list_observer,
    Mode mode)
    : browser_list_(browser_list),
      web_state_list_observer_(std::move(web_state_list_observer)),
      scoped_observations_(web_state_list_observer_.get()),
      mode_(mode) {
  browser_list_->AddObserver(this);

  const BrowserList::BrowserType browser_types = BrowserTypesForMode(mode_);

  // There may already be browsers in `browser_list` when this object is
  // created. Register as an observer for (mode permitting) both the regular and
  // incognito browsers' WebStateLists.
  for (Browser* browser : browser_list_->BrowsersOfType(browser_types)) {
    scoped_observations_.AddObservation(browser->GetWebStateList());
  }
}

AllWebStateListObservationRegistrar::AllWebStateListObservationRegistrar(
    BrowserList* browser_list,
    std::unique_ptr<WebStateListObserver> web_state_list_observer)
    : AllWebStateListObservationRegistrar(browser_list,
                                          std::move(web_state_list_observer),
                                          Mode::ALL) {}

AllWebStateListObservationRegistrar::~AllWebStateListObservationRegistrar() {
  // If the owning profile has already shut down, `browser_list_` should
  // be nullptr; otherwise, stop observing it.
  if (browser_list_) {
    browser_list_->RemoveObserver(this);
  }
}

void AllWebStateListObservationRegistrar::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (ShouldPropagateEvent(browser->type(), mode_)) {
    scoped_observations_.AddObservation(browser->GetWebStateList());
  }
}

void AllWebStateListObservationRegistrar::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (ShouldPropagateEvent(browser->type(), mode_)) {
    scoped_observations_.RemoveObservation(browser->GetWebStateList());
  }
}

void AllWebStateListObservationRegistrar::OnBrowserListShutdown(
    BrowserList* browser_list) {
  DCHECK_EQ(browser_list, browser_list_);
  // Stop observing all observed web state lists.
  scoped_observations_.RemoveAllObservations();

  // Stop observimg the browser list, and clear `browser_list_`.
  browser_list_->RemoveObserver(this);
  browser_list_ = nullptr;
}
