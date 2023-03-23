// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class HTMLElement;
class Element;
class IdTargetObserver;

// Tracks the value of HTMLElement::anchorElement() to help other elements know
// whether they are used as implicit anchor elements.
// NOTE: this class is unrelated to the <a> element.
class AnchorElementObserver : public GarbageCollected<AnchorElementObserver>,
                              public ElementRareDataField {
 public:
  explicit AnchorElementObserver(HTMLElement* element) : element_(element) {
    DCHECK(element_);
  }
  const HTMLElement& GetElement() const { return *element_; }

  void Notify();

  void Trace(Visitor* visitor) const override;

 private:
  void ResetIdTargetObserverIfNeeded();

  Member<HTMLElement> element_;
  Member<Element> anchor_;
  Member<IdTargetObserver> id_target_observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_H_
