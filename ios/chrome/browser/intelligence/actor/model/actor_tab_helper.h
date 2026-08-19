// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_

#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

class ActorTabHelperObserver;

// `ActorTabHelper` is a tab helper used to track actuation state for an
// `ActorTask` on a `WebState`.
class ActorTabHelper : public web::WebStateUserData<ActorTabHelper> {
 public:
  // When actuation state changes, this callback is invoked with whether the
  // associated tab is actively being actuated.
  using ActuationStateCallbackList = base::RepeatingCallbackList<void(bool)>;
  using ActuationStateCallback = ActuationStateCallbackList::CallbackType;

  ActorTabHelper(const ActorTabHelper&) = delete;
  ActorTabHelper& operator=(const ActorTabHelper&) = delete;
  ActorTabHelper(ActorTabHelper&&) = delete;
  ActorTabHelper& operator=(ActorTabHelper&&) = delete;

  ~ActorTabHelper() override;

  // Sets whether the tab is actively undergoing actuation by an `ActorTask`.
  void SetActuating(bool actuating);

  // Returns true if the tab is currently being actuated.
  bool IsActuating() const;

  // Registers a callback to be invoked when the actuation state changes.
  // The returned subscription manages the lifetime of the registration and
  // must be retained by callers.
  [[nodiscard]] base::CallbackListSubscription AddActuationStateChangedCallback(
      ActuationStateCallback callback);

  // Adds an observer that will be called on actuating state changes.
  void AddObserver(ActorTabHelperObserver* observer);

  // Removes `observer` from the list of observers.
  void RemoveObserver(ActorTabHelperObserver* observer);

 private:
  friend class web::WebStateUserData<ActorTabHelper>;
  explicit ActorTabHelper(web::WebState* web_state);

  // Whether the tab is actively undergoing actuation by an actor
  // task. It is not actuated until an actor task starts working on it.
  bool is_actuating_ = false;

  // The `WebState` associated with the `ActorTabHelper`. Outlives the helper
  // since the helper's lifetime is bound to the user data of the `WebState`.
  raw_ptr<web::WebState> web_state_;

  // The list of observers registered to receive notifications.
  base::ObserverList<ActorTabHelperObserver> observers_;

  // The list of callbacks registered to receive actuation state changes.
  ActuationStateCallbackList actuation_state_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_
