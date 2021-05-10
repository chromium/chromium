// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_COUNTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_COUNTS_H_

#include "third_party/blink/renderer/bindings/core/v8/maplike.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class EventCounts final : public ScriptWrappable,
                          public Maplike<AtomicString, unsigned> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  EventCounts();

  const HashMap<AtomicString, unsigned>& Map() const {
    return event_count_map_;
  }

  // IDL attributes / methods
  uint32_t size() const { return event_count_map_.size(); }

  void Add(const AtomicString& event_type);

  // Add multiple events with the same event type.
  void AddMultipleEvents(const AtomicString& event_type, unsigned count);

  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
  }

 private:
  // Maplike implementation.
  PairIterable<AtomicString, unsigned>::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override;
  bool GetMapEntry(ScriptState*,
                   const AtomicString& key,
                   unsigned& value,
                   ExceptionState&) override;

  HashMap<AtomicString, unsigned> event_count_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_EVENT_COUNTS_H_
