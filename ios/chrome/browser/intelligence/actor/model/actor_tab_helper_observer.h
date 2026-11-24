// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_

#import "base/observer_list_types.h"

namespace actor {
enum class ActorControlState;
}  // namespace actor

namespace web {
class WebState;
}  // namespace web

class ActorTabHelper;

// `ActorTabHelperObserver` is the observer interface for `ActorTabHelper`.
class ActorTabHelperObserver : public base::CheckedObserver {
 public:
  // Notifies the observer when the `control_state` of `tab_helper`
  // associated with `web_state` changes from `previous_control_state` to
  // `new_control_state`.
  virtual void OnControlStateChanged(
      ActorTabHelper* tab_helper,
      web::WebState* web_state,
      actor::ActorControlState previous_control_state,
      actor::ActorControlState new_control_state) = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_
