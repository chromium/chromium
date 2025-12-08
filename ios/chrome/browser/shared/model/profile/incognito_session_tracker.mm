// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/incognito_session_tracker.h"

#import <algorithm>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_incognito_session_tracker.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

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

void IncognitoSessionTracker::OnProfileManagerWillBeDestroyed(
    ProfileManagerIOS* manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do, IncognitoSessionTracker does not keep any profile alive.
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
  auto [_, inserted] = trackers_.insert(std::make_pair(
      profile, std::make_unique<ProfileIncognitoSessionTracker>(
                   BrowserListFactory::GetForProfile(profile),
                   base::BindRepeating(
                       &IncognitoSessionTracker::OnIncognitoSessionStateChanged,
                       base::Unretained(this)))));

  DCHECK(inserted);
}

void IncognitoSessionTracker::OnProfileUnloaded(ProfileManagerIOS* manager,
                                                ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iterator = trackers_.find(profile);
  DCHECK(iterator != trackers_.end());
  trackers_.erase(iterator);

  // The removed profile does not have any tabs, thus no incognito tabs.
  OnIncognitoSessionStateChanged(false);
}

void IncognitoSessionTracker::OnProfileMarkedForPermanentDeletion(
    ProfileManagerIOS* manager,
    ProfileIOS* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do, OnProfileUnloaded(...) will take care of removing the
  // observer when it is called.
}

void IncognitoSessionTracker::OnIncognitoSessionStateChanged(
    bool has_incognito_tabs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool has_incognito_session_tabs =
      has_incognito_tabs ||
      std::ranges::any_of(
          trackers_, &ProfileIncognitoSessionTracker::has_incognito_tabs,
          [](auto& pair) -> const ProfileIncognitoSessionTracker& {
            return *pair.second;
          });

  if (has_incognito_session_tabs_ != has_incognito_session_tabs) {
    has_incognito_session_tabs_ = has_incognito_session_tabs;
    callbacks_.Notify(has_incognito_session_tabs_);
  }
}
