// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"

#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

ObservationDelayController::ObservationDelayController(
    ActorTaskId task_id,
    AggregatedJournal* journal) {
  // TODO(crbug.com/498991756): fill out this skeletion implementation.
}

void ObservationDelayController::Wait(base::WeakPtr<web::WebFrame> web_frame,
                                      ReadyCallback callback) {
  // TODO(crbug.com/498991756): fill out this skeletion implementation.
}

}  // namespace actor
