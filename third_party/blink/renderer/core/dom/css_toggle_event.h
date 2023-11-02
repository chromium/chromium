// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class CSSToggle;
class CSSToggleEventInit;

class CORE_EXPORT CSSToggleEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSToggleEvent* Create(
      const AtomicString& type,
      const CSSToggleEventInit* initializer = nullptr) {
    return MakeGarbageCollected<CSSToggleEvent>(type, initializer);
  }
  static CSSToggleEvent* Create(const AtomicString& type,
                                const AtomicString& toggle_name,
                                CSSToggle* toggle) {
    return MakeGarbageCollected<CSSToggleEvent>(type, toggle_name, toggle);
  }

  CSSToggleEvent(const AtomicString& type, const CSSToggleEventInit*);
  CSSToggleEvent(const AtomicString& type,
                 const AtomicString& toggle_name,
                 CSSToggle* toggle);

  String toggleName() const { return toggle_name_; }
  CSSToggle* toggle() const { return toggle_; }

  void Trace(Visitor*) const override;

 private:
  String toggle_name_;
  Member<CSSToggle> toggle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_EVENT_H_
