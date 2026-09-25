// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_CONTROL_STATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_CONTROL_STATE_H_

#import <ostream>
#import <string_view>

namespace actor {

// Represents the control state of a tab or WebState in relation to an
// ActorTask.
enum class ActorControlState {
  // Tab has no active actor task.
  kInactive = 0,
  // Tab is actively controlled by the actor.
  kActorControlled = 1,
};

// Returns a string representation of `control_state`.
constexpr std::string_view ActorControlStateToString(
    ActorControlState control_state) {
  switch (control_state) {
    case ActorControlState::kInactive:
      return "kInactive";
    case ActorControlState::kActorControlled:
      return "kActorControlled";
  }
  return "kUnknown";
}

// Stream operator for logging and test assertions.
inline std::ostream& operator<<(std::ostream& os,
                                ActorControlState control_state) {
  return os << ActorControlStateToString(control_state);
}

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_CONTROL_STATE_H_
