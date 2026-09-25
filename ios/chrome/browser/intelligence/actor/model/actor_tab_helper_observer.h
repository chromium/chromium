// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_

#import "base/observer_list_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"

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
      actor::ActorControlState new_control_state);

  // Notifies the observer when the actuation state of `tab_helper` changes for
  // `web_state`. `actuating` indicates the new actuation state.
  // TODO(crbug.com/548051839): Deprecated, remove once callers are updated.
  virtual void OnActuationStateChanged(ActorTabHelper* tab_helper,
                                       web::WebState* web_state,
                                       bool actuating) {}
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_TAB_HELPER_OBSERVER_H_
