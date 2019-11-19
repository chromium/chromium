// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PENDING_USER_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PENDING_USER_INPUT_H_

#include <array>

#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/pending_user_input_type.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace scheduler {

class PLATFORM_EXPORT PendingUserInput {
  DISALLOW_NEW();

 public:
  // Handles the dispatch of WebInputEvents from the main thread scheduler,
  // keeping track of the set of in-flight input events.
  class PLATFORM_EXPORT Monitor {
    DISALLOW_NEW();

   public:
    Monitor() { this->counters_.fill(0); }
    void OnEnqueue(WebInputEvent::Type);
    void OnDequeue(WebInputEvent::Type);

    PendingUserInputInfo Info() const;

   private:
    std::array<size_t, WebInputEvent::kTypeLast + 1> counters_;

    DISALLOW_COPY_AND_ASSIGN(Monitor);
  };

  PendingUserInput() = delete;

  static PendingUserInputType TypeFromString(const AtomicString&);
  static PendingUserInputType TypeFromWebInputEventType(WebInputEvent::Type);

  DISALLOW_COPY_AND_ASSIGN(PendingUserInput);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_PENDING_USER_INPUT_H_
