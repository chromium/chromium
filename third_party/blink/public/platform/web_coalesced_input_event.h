// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COALESCED_INPUT_EVENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COALESCED_INPUT_EVENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"

#include <memory>
#include <vector>

namespace blink {

// This class is representing a polymorphic WebInputEvent structure with its
// coalesced events. The event could be any events defined in WebInputEvent.h.
class BLINK_PLATFORM_EXPORT WebCoalescedInputEvent {
 public:
  explicit WebCoalescedInputEvent(const WebInputEvent&);
  WebCoalescedInputEvent(const WebInputEvent&,
                         const std::vector<const WebInputEvent*>&,
                         const std::vector<const WebInputEvent*>&);
  WebCoalescedInputEvent(const WebPointerEvent&,
                         const std::vector<WebPointerEvent>&,
                         const std::vector<WebPointerEvent>&);
  // Copy constructor to deep copy the event.
  WebCoalescedInputEvent(const WebCoalescedInputEvent&);

  WebInputEvent* EventPointer();
  void AddCoalescedEvent(const blink::WebInputEvent&);
  const WebInputEvent& Event() const;
  size_t CoalescedEventSize() const;
  const WebInputEvent& CoalescedEvent(size_t index) const;
  std::vector<const WebInputEvent*> GetCoalescedEventsPointers() const;

  void AddPredictedEvent(const blink::WebInputEvent&);
  size_t PredictedEventSize() const;
  const WebInputEvent& PredictedEvent(size_t index) const;
  std::vector<const WebInputEvent*> GetPredictedEventsPointers() const;

 private:
  // TODO(hans): Remove this once clang-cl knows to not inline dtors that
  // call operator(), https://crbug.com/691714
  struct BLINK_PLATFORM_EXPORT WebInputEventDeleter {
    void operator()(blink::WebInputEvent*) const;
  };

  using WebScopedInputEvent =
      std::unique_ptr<WebInputEvent, WebInputEventDeleter>;

  WebScopedInputEvent MakeWebScopedInputEvent(const blink::WebInputEvent&);

  WebScopedInputEvent event_;
  std::vector<WebScopedInputEvent> coalesced_events_;
  std::vector<WebScopedInputEvent> predicted_events_;
};

using WebScopedCoalescedInputEvent = std::unique_ptr<WebCoalescedInputEvent>;

}  // namespace blink

#endif
