// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/popover_animation_finished_event_listener.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

enum class PopoverVisibilityState {
  kHidden,
  kTransitioning,
  kShowing,
};

class PopoverAnchorObserver : public IdTargetObserver {
 public:
  PopoverAnchorObserver(const AtomicString& id, HTMLElement* element)
      : IdTargetObserver(element->GetTreeScope().GetIdTargetObserverRegistry(),
                         id),
        element_(element) {}

  void IdTargetChanged() override { element_->PopoverAnchorElementChanged(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    IdTargetObserver::Trace(visitor);
  }

 private:
  Member<HTMLElement> element_;
};

class PopoverData final : public GarbageCollected<PopoverData>,
                          public ElementRareDataField {
 public:
  PopoverData() = default;
  PopoverData(const PopoverData&) = delete;
  PopoverData& operator=(const PopoverData&) = delete;

  PopoverVisibilityState visibilityState() const { return visibility_state_; }
  void setVisibilityState(PopoverVisibilityState visibility_state) {
    visibility_state_ = visibility_state;
  }

  PopoverValueType type() const { return type_; }
  void setType(PopoverValueType type) {
    type_ = type;
    DCHECK_NE(type, PopoverValueType::kNone)
        << "Remove PopoverData rather than setting kNone type";
  }

  Element* invoker() const { return invoker_; }
  void setInvoker(Element* element) { invoker_ = element; }

  Element* previouslyFocusedElement() const {
    return previously_focused_element_;
  }
  void setPreviouslyFocusedElement(Element* element) {
    previously_focused_element_ = element;
  }

  PopoverAnimationFinishedEventListener* animationFinishedListener() const {
    return animation_finished_listener_;
  }
  void setAnimationFinishedListener(
      PopoverAnimationFinishedEventListener* listener) {
    if (animation_finished_listener_ &&
        !animation_finished_listener_->IsFinished()) {
      // If we're clearing the listener, dispose it, to prevent listeners from
      // firing later.
      animation_finished_listener_->Dispose();
    }
    DCHECK(!animation_finished_listener_ ||
           animation_finished_listener_->IsFinished());
    animation_finished_listener_ = listener;
  }

  void setAnchorElement(Element* anchor) { anchor_element_ = anchor; }
  Element* anchorElement() const { return anchor_element_; }
  void setAnchorObserver(PopoverAnchorObserver* observer) {
    anchor_observer_ = observer;
  }

  HTMLSelectMenuElement* ownerSelectMenuElement() const {
    return owner_select_menu_element_;
  }
  void setOwnerSelectMenuElement(HTMLSelectMenuElement* element) {
    owner_select_menu_element_ = element;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(invoker_);
    visitor->Trace(previously_focused_element_);
    visitor->Trace(animation_finished_listener_);
    visitor->Trace(anchor_element_);
    visitor->Trace(anchor_observer_);
    visitor->Trace(owner_select_menu_element_);
    ElementRareDataField::Trace(visitor);
  }

 private:
  PopoverVisibilityState visibility_state_ = PopoverVisibilityState::kHidden;
  PopoverValueType type_ = PopoverValueType::kNone;
  WeakMember<Element> invoker_;
  WeakMember<Element> previously_focused_element_;
  // We hold a strong reference to the animation finished listener, so that we
  // can confirm that the listeners get removed before cleanup.
  Member<PopoverAnimationFinishedEventListener> animation_finished_listener_;

  // Target of the 'anchor' attribute.
  Member<Element> anchor_element_;
  Member<PopoverAnchorObserver> anchor_observer_;

  WeakMember<HTMLSelectMenuElement> owner_select_menu_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_
