// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_queue_test_helper.h"

#include "services/ws/event_queue.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_test_helper.h"

namespace ws {

EventQueueTestHelper::EventQueueTestHelper(EventQueue* event_queue)
    : event_queue_(event_queue) {}

EventQueueTestHelper::~EventQueueTestHelper() = default;

bool EventQueueTestHelper::HasInFlightEvent() const {
  return event_queue_->in_flight_event_.has_value();
}

void EventQueueTestHelper::AckInFlightEvent() {
  DCHECK(HasInFlightEvent());
  for (WindowTree* window_tree :
       event_queue_->window_service_->window_trees()) {
    if (window_tree->client_id() == event_queue_->in_flight_event_->client_id) {
      WindowTreeTestHelper(window_tree)
          .OnWindowInputEventAck(event_queue_->in_flight_event_->event_id,
                                 mojom::EventResult::HANDLED);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace ws
