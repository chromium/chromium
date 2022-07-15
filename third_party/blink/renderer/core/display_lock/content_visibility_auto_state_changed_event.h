// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_CONTENT_VISIBILITY_AUTO_STATE_CHANGED_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_CONTENT_VISIBILITY_AUTO_STATE_CHANGED_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class ContentVisibilityAutoStateChangedEventInit;

class ContentVisibilityAutoStateChangedEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ContentVisibilityAutoStateChangedEvent* Create() {
    return MakeGarbageCollected<ContentVisibilityAutoStateChangedEvent>();
  }
  static ContentVisibilityAutoStateChangedEvent* Create(
      const AtomicString& type,
      bool skipped) {
    return MakeGarbageCollected<ContentVisibilityAutoStateChangedEvent>(
        type, skipped);
  }
  static ContentVisibilityAutoStateChangedEvent* Create(
      const AtomicString& type,
      const ContentVisibilityAutoStateChangedEventInit* initializer) {
    return MakeGarbageCollected<ContentVisibilityAutoStateChangedEvent>(
        type, initializer);
  }

  ContentVisibilityAutoStateChangedEvent();
  ContentVisibilityAutoStateChangedEvent(const AtomicString& type,
                                         bool skipped);
  ContentVisibilityAutoStateChangedEvent(
      const AtomicString&,
      const ContentVisibilityAutoStateChangedEventInit*);
  ~ContentVisibilityAutoStateChangedEvent() override;

  bool skipped() const;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  bool skipped_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DISPLAY_LOCK_CONTENT_VISIBILITY_AUTO_STATE_CHANGED_EVENT_H_
