// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_observer.h"

#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

class AnchorIdTargetObserver : public IdTargetObserver {
 public:
  AnchorIdTargetObserver(const AtomicString& id,
                         AnchorElementObserver* anchor_element_observer)
      : IdTargetObserver(anchor_element_observer->GetSourceElement()
                             .GetTreeScope()
                             .EnsureIdTargetObserverRegistry(),
                         id),
        anchor_element_observer_(anchor_element_observer) {}

  void IdTargetChanged() override { anchor_element_observer_->Notify(); }

  bool SameId(const AtomicString& id) const { return id == Id(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(anchor_element_observer_);
    IdTargetObserver::Trace(visitor);
  }

 private:
  Member<AnchorElementObserver> anchor_element_observer_;
};

bool NeedsIdTargetObserver(Element& element) {
  return element.IsInTreeScope() &&
         !element.HasExplicitlySetAttrAssociatedElements(
             html_names::kAnchorAttr) &&
         !element.FastGetAttribute(html_names::kAnchorAttr).empty();
}

}  // namespace

void AnchorElementObserver::Trace(Visitor* visitor) const {
  visitor->Trace(source_element_);
  visitor->Trace(current_anchor_);
  visitor->Trace(id_target_observer_);
  ElementRareDataField::Trace(visitor);
}

void AnchorElementObserver::Notify() {
  Element* new_anchor = source_element_->anchorElement();
  if (current_anchor_ != new_anchor) {
    if (current_anchor_) {
      current_anchor_->DecrementImplicitlyAnchoredElementCount();
    }
    if (new_anchor) {
      new_anchor->IncrementImplicitlyAnchoredElementCount();
    }
    current_anchor_ = new_anchor;
    if (source_element_->GetLayoutObject()) {
      source_element_->GetLayoutObject()
          ->SetNeedsLayoutAndFullPaintInvalidation(
              layout_invalidation_reason::kAnchorPositioning);
    }
  }
  ResetIdTargetObserverIfNeeded();
}

void AnchorElementObserver::ResetIdTargetObserverIfNeeded() {
  if (!NeedsIdTargetObserver(*source_element_)) {
    if (id_target_observer_) {
      id_target_observer_->Unregister();
      id_target_observer_ = nullptr;
    }
    return;
  }
  const AtomicString& anchor_id =
      source_element_->FastGetAttribute(html_names::kAnchorAttr);
  if (id_target_observer_) {
    if (static_cast<AnchorIdTargetObserver*>(id_target_observer_.Get())
            ->SameId(anchor_id)) {
      // Already observing the same id target. Nothing more to do.
      return;
    }
    id_target_observer_->Unregister();
  }
  id_target_observer_ =
      MakeGarbageCollected<AnchorIdTargetObserver>(anchor_id, this);
}

}  // namespace blink
