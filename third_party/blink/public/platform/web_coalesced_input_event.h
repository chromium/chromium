// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COALESCED_INPUT_EVENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COALESCED_INPUT_EVENT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_vector.h"

#include <memory>

namespace blink {

// This class represents a polymorphic WebInputEvent structure with its
// coalesced events. The event could be any event defined in web_input_event.h,
// including those that cannot be coalesced.
class BLINK_PLATFORM_EXPORT WebCoalescedInputEvent {
 public:
  explicit WebCoalescedInputEvent(const WebInputEvent&);
  WebCoalescedInputEvent(const WebInputEvent&,
                         const WebVector<const WebInputEvent*>&,
                         const WebVector<const WebInputEvent*>&);
  WebCoalescedInputEvent(const WebPointerEvent&,
                         const WebVector<WebPointerEvent>&,
                         const WebVector<WebPointerEvent>&);
  // Copy constructor to deep copy the event.
  WebCoalescedInputEvent(const WebCoalescedInputEvent&);

  WebInputEvent* EventPointer();
  void AddCoalescedEvent(const blink::WebInputEvent&);
  const WebInputEvent& Event() const;
  size_t CoalescedEventSize() const;
  const WebInputEvent& CoalescedEvent(size_t index) const;
  WebVector<const WebInputEvent*> GetCoalescedEventsPointers() const;

  void AddPredictedEvent(const blink::WebInputEvent&);
  size_t PredictedEventSize() const;
  const WebInputEvent& PredictedEvent(size_t index) const;
  WebVector<const WebInputEvent*> GetPredictedEventsPointers() const;

 private:
  struct BLINK_PLATFORM_EXPORT WebInputEventDeleter {
    void operator()(blink::WebInputEvent*) const;
  };

  using WebScopedInputEvent =
      std::unique_ptr<WebInputEvent, WebInputEventDeleter>;

  WebScopedInputEvent MakeWebScopedInputEvent(const blink::WebInputEvent&);

  WebScopedInputEvent event_;
  WebVector<WebScopedInputEvent> coalesced_events_;
  WebVector<WebScopedInputEvent> predicted_events_;
};

using WebScopedCoalescedInputEvent = std::unique_ptr<WebCoalescedInputEvent>;

}  // namespace blink

#endif
