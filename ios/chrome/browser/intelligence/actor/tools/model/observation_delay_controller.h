// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace web {

class WebFrame;
}  // namespace web

namespace actor {

class AggregatedJournal;

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for an observation.
//
// Mirrored from the desktop equivalent at:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/tools/observation_delay_controller.h;l=39;drc=5948d13accdd1c0a814b950a7b4b12b84fca004a
class ObservationDelayController {
 public:
  enum class Result {
    kOk,
    // This is returned if the primary main frame starts a new navigation
    // while we are waiting.
    kPageNavigated,
  };

  using ReadyCallback = base::OnceCallback<void(Result)>;

  ObservationDelayController(ActorTaskId task_id, AggregatedJournal* journal);

  // Waits for page stability on the given `web_frame`, returning the result via
  // `callback`.
  void Wait(base::WeakPtr<web::WebFrame> web_frame, ReadyCallback callback);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_OBSERVATION_DELAY_CONTROLLER_H_
