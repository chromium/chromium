// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WAIT_FOR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WAIT_FOR_EVENT_H_

#include "base/run_loop.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"

namespace blink {

class Element;

// Helper class that will block running the test until the given event is fired
// on the given element.
class WaitForEvent : public NativeEventListener {
 public:
  WaitForEvent(Element*, const AtomicString&);

  void Invoke(ExecutionContext*, Event*) final;

  void Trace(Visitor*) final;

 private:
  base::RunLoop run_loop_;
  Member<Element> element_;
  AtomicString event_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_WAIT_FOR_EVENT_H_
