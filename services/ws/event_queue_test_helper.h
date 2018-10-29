// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_QUEUE_TEST_HELPER_H_
#define SERVICES_WS_EVENT_QUEUE_TEST_HELPER_H_

#include "base/macros.h"

namespace ws {

class EventQueue;

// Used for accessing private members of EventQueue in tests.
class EventQueueTestHelper {
 public:
  explicit EventQueueTestHelper(EventQueue* event_queue);
  ~EventQueueTestHelper();

  bool HasInFlightEvent() const;

  void AckInFlightEvent();

 private:
  EventQueue* event_queue_;

  DISALLOW_COPY_AND_ASSIGN(EventQueueTestHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_EVENT_QUEUE_TEST_HELPER_H_
