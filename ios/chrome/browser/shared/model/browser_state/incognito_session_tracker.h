// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_INCOGNITO_SESSION_TRACKER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_INCOGNITO_SESSION_TRACKER_H_

#import "base/callback_list.h"
#import "base/containers/flat_map.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ProfileManagerIOS;

// Tracks whether there are any open off-the-record tabs open by any Profile in
// the application. Allow to be notified when the state changes or to poll it
// when needed.
class IncognitoSessionTracker final : public ProfileManagerObserverIOS {
 public:
  // Container for the callbacks registered with RegisterCallback().
  using SessionStateChangedCallbackList =
      base::RepeatingCallbackList<void(bool)>;

  // Type of the callbacks registered with RegisterCallback().
  using SessionStateChangedCallback =
      SessionStateChangedCallbackList::CallbackType;

  explicit IncognitoSessionTracker(ProfileManagerIOS* manager);
  ~IncognitoSessionTracker() final;

  // Returns whether there are any off-the-record tabs open.
  bool HasIncognitoSessionTabs() const;

  // Registers `callback` to be invoked when the presence of off-the-record
  // tabs has changed (e.g. the callback will not be called if the number of
  // open off-the-record tabs goes from two to three).
  base::CallbackListSubscription RegisterCallback(
      SessionStateChangedCallback callback);

  // ProfileManagerObserverIOS:
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override;

 private:
  // Forward-declaration of the observer used to track the state of
  // an individual Profile.
  class Observer;

  // Invoked when the state of invoked when the presence of off-the-record
  // tabs for a specific Profile has changed.
  void OnIncognitoSessionStateChanged(bool has_incognito_tabs);

  // Manage observing the ProfileManagerIOS.
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      scoped_manager_observation_{this};

  // Map from Profile to the observer used to track whether it has any open
  // off-the-record tabs.
  base::flat_map<ProfileIOS*, std::unique_ptr<Observer>> observers_;

  // List of registered callbacks.
  SessionStateChangedCallbackList callbacks_;

  // Whether any off-the-record tabs are open in any Profile.
  bool has_incognito_session_tabs_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_INCOGNITO_SESSION_TRACKER_H_
