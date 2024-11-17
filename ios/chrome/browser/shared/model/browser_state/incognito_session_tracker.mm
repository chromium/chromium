// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser_state/incognito_session_tracker.h"

#import "base/ranges/algorithm.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

// Observer used to track the state of an individual Profile
// and inform the IncognitoSessionTracker when the state of the incognito
// session for that Profile changes.
class IncognitoSessionTracker::Observer final : public BrowserListObserver,
                                                public WebStateListObserver {
 public:
  // Callback invoked when the presence of off-the-record tabs has changed.
  using Callback = base::RepeatingCallback<void(bool)>;

  Observer(BrowserList* list, Callback callback);
  ~Observer() final;

  // Returns whether any of the BrowserList's Browser has incognito tabs open.
  bool has_incognito_tabs() const { return has_incognito_tabs_; }

  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* list, Browser* browser) final;
  void OnBrowserRemoved(const BrowserList* list, Browser* browser) final;
  void OnBrowserListShutdown(BrowserList* list) final;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;
  void BatchOperationEnded(WebStateList* web_state_list) final;

 private:
  // Invoked when a potentially significant change is detected in any of
  // the observed WebStateList.
  void OnWebStateListChanged();

  // Closure invoked when the presence of off-the-record tabs has changed.
  Callback callback_;

  // Manages the observation of the BrowserList.
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};

  // Manages the observation of all off-the-record WebStateLists.
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};

  // Whether any of the WebStateList has an off-the-record tab open.
  bool has_incognito_tabs_ = false;
};

IncognitoSessionTracker::Observer::Observer(BrowserList* browser_list,
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

IncognitoSessionTracker::Observer::~Observer() = default;

void IncognitoSessionTracker::Observer::OnBrowserAdded(
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

void IncognitoSessionTracker::Observer::OnBrowserRemoved(
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

void IncognitoSessionTracker::Observer::WebStateListDidChange(
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

void IncognitoSessionTracker::Observer::OnWebStateListChanged() {
  const bool has_incognito_tabs = base::ranges::any_of(
      web_state_list_observations_.sources(),
      [](WebStateList* web_state_list) { return !web_state_list->empty(); });

  if (has_incognito_tabs_ != has_incognito_tabs) {
    has_incognito_tabs_ = has_incognito_tabs;
    callback_.Run(has_incognito_tabs_);
  }
}

void IncognitoSessionTracker::Observer::BatchOperationEnded(
    WebStateList* web_state_list) {
  // Anything can change during a batch operation. Update the state.
  OnWebStateListChanged();
}

void IncognitoSessionTracker::Observer::OnBrowserListShutdown(
    BrowserList* browser_list) {
  browser_list_observation_.Reset();
}

IncognitoSessionTracker::IncognitoSessionTracker(ProfileManagerIOS* manager) {
  // ProfileManagerIOS invoke OnProfileLoaded(...) for all Profiles already
  // loaded, so there is no need to manually iterate over them.
  scoped_manager_observation_.Observe(manager);
}

IncognitoSessionTracker::~IncognitoSessionTracker() = default;

bool IncognitoSessionTracker::HasIncognitoSessionTabs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_incognito_session_tabs_;
}

base::CallbackListSubscription IncognitoSessionTracker::RegisterCallback(
    SessionStateChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.Add(std::move(callback));
}

void IncognitoSessionTracker::OnProfileManagerDestroyed(
    ProfileManagerIOS* manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_manager_observation_.Reset();
}

void IncognitoSessionTracker::OnProfileCreated(ProfileManagerIOS* manager,
                                               ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The Profile is still not fully loaded, so the KeyedService cannot be
  // accessed (and it may be destroyed before the load complete). Wait until the
  // end of the initialisation before tracking its session.
}

void IncognitoSessionTracker::OnProfileLoaded(ProfileManagerIOS* manager,
                                              ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The Profile is fully loaded, we can access its BrowserList and register an
  // Observer. The use of `base::Unretained(this)` is safe as the
  // `IncognitoSessionTracker` owns the `Observer` and the closure cannot
  // outlive `this`.
  auto [_, inserted] = observers_.insert(std::make_pair(
      profile, std::make_unique<Observer>(
                   BrowserListFactory::GetForProfile(profile),
                   base::BindRepeating(
                       &IncognitoSessionTracker::OnIncognitoSessionStateChanged,
                       base::Unretained(this)))));

  DCHECK(inserted);
}

void IncognitoSessionTracker::OnIncognitoSessionStateChanged(
    bool has_incognito_tabs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool has_incognito_session_tabs =
      has_incognito_tabs ||
      base::ranges::any_of(
          observers_, &Observer::has_incognito_tabs,
          [](auto& pair) -> const Observer& { return *pair.second; });

  if (has_incognito_session_tabs_ != has_incognito_session_tabs) {
    has_incognito_session_tabs_ = has_incognito_session_tabs;
    callbacks_.Notify(has_incognito_session_tabs_);
  }
}
