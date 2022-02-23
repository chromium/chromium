// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_CURRENT_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_CURRENT_CHANGE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class AppHistoryCurrentChangeEventInit;
class AppHistoryEntry;

class AppHistoryCurrentChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AppHistoryCurrentChangeEvent* Create(
      const AtomicString& type,
      AppHistoryCurrentChangeEventInit* init) {
    return MakeGarbageCollected<AppHistoryCurrentChangeEvent>(type, init);
  }

  AppHistoryCurrentChangeEvent(const AtomicString& type,
                               AppHistoryCurrentChangeEventInit* init);

  String navigationType() { return navigation_type_; }
  AppHistoryEntry* from() { return from_; }

  const AtomicString& InterfaceName() const final;
  void Trace(Visitor* visitor) const final;

 private:
  String navigation_type_;
  Member<AppHistoryEntry> from_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_CURRENT_CHANGE_EVENT_H_
