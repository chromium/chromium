// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_

#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

class ActorTabHelperObserver;

// `ActorTabHelper` is a tab helper used to track Actor-related state that is
// tab-bound.
class ActorTabHelper : public web::WebStateUserData<ActorTabHelper> {
 public:
  // When the `ActorControlState` changes, this callback is invoked with
  // the previous and new control states.
  using ControlStateCallbackList =
      base::RepeatingCallbackList<void(actor::ActorControlState,
                                       actor::ActorControlState)>;
  using ControlStateCallback = ControlStateCallbackList::CallbackType;

  // When actuation state changes, this callback is invoked with whether the
  // associated tab is actively being actuated.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  using ActuationStateCallbackList = base::RepeatingCallbackList<void(bool)>;
  using ActuationStateCallback = ActuationStateCallbackList::CallbackType;

  ActorTabHelper(const ActorTabHelper&) = delete;
  ActorTabHelper& operator=(const ActorTabHelper&) = delete;
  ActorTabHelper(ActorTabHelper&&) = delete;
  ActorTabHelper& operator=(ActorTabHelper&&) = delete;

  ~ActorTabHelper() override;

  // Sets the `ActorControlState` for the associated `WebState`,
  // notifying registered callbacks and observers if the control state changes.
  void SetControlState(actor::ActorControlState control_state);

  // Sets whether the tab is actively undergoing actuation by an `ActorTask`.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  void SetActuating(bool actuating);

  // Returns the current `ActorControlState` of the associated
  // `WebState`.
  actor::ActorControlState GetControlState() const;

  // Returns true if the tab is currently being actuated.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  bool IsActuating() const;

  // Registers a callback to be invoked when the `ActorControlState`
  // changes. The returned subscription manages the lifetime of the
  // registration and must be retained by callers.
  [[nodiscard]] base::CallbackListSubscription AddControlStateChangedCallback(
      ControlStateCallback callback);

  // Registers a callback to be invoked when the actuation state changes.
  // The returned subscription manages the lifetime of the registration and
  // must be retained by callers.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  [[nodiscard]] base::CallbackListSubscription AddActuationStateChangedCallback(
      ActuationStateCallback callback);

  // Adds an observer that will be called on `ActorControlState` changes.
  void AddObserver(ActorTabHelperObserver* observer);

  // Removes `observer` from the list of observers.
  void RemoveObserver(ActorTabHelperObserver* observer);

 private:
  friend class web::WebStateUserData<ActorTabHelper>;
  explicit ActorTabHelper(web::WebState* web_state);

  // The current actor control state of the associated WebState.
  actor::ActorControlState control_state_ = actor::ActorControlState::kInactive;

  // The `WebState` associated with the `ActorTabHelper`. Outlives the helper
  // since the helper's lifetime is bound to the user data of the `WebState`.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // The list of observers registered to receive notifications.
  base::ObserverList<ActorTabHelperObserver> observers_;

  // The list of callbacks registered to receive `ActorControlState` changes.
  ControlStateCallbackList control_state_callbacks_;

  // The list of callbacks registered to receive actuation state changes.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  ActuationStateCallbackList actuation_state_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_H_
