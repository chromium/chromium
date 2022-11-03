// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/element.h"
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

class PopoverData final : public GarbageCollected<PopoverData> {
 public:
  PopoverData() = default;
  PopoverData(const PopoverData&) = delete;
  PopoverData& operator=(const PopoverData&) = delete;

  bool hadDefaultOpenWhenParsed() const { return had_defaultopen_when_parsed_; }
  void setHadDefaultOpenWhenParsed(bool value) {
    had_defaultopen_when_parsed_ = value;
  }

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

  void setNeedsRepositioningForSelectMenu(bool flag) {
    needs_repositioning_for_select_menu_ = flag;
  }
  bool needsRepositioningForSelectMenu() {
    return needs_repositioning_for_select_menu_;
  }

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

  HTMLSelectMenuElement* ownerSelectMenuElement() const {
    return owner_select_menu_element_;
  }
  void setOwnerSelectMenuElement(HTMLSelectMenuElement* element) {
    owner_select_menu_element_ = element;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(invoker_);
    visitor->Trace(previously_focused_element_);
    visitor->Trace(animation_finished_listener_);
    visitor->Trace(owner_select_menu_element_);
  }

 private:
  bool had_defaultopen_when_parsed_ = false;
  PopoverVisibilityState visibility_state_ = PopoverVisibilityState::kHidden;
  PopoverValueType type_ = PopoverValueType::kNone;
  WeakMember<Element> invoker_;
  WeakMember<Element> previously_focused_element_;
  // We hold a strong reference to the animation finished listener, so that we
  // can confirm that the listeners get removed before cleanup.
  Member<PopoverAnimationFinishedEventListener> animation_finished_listener_;

  // TODO(crbug.com/1197720): The popover position should be provided by the new
  // anchored positioning scheme.
  bool needs_repositioning_for_select_menu_ = false;
  WeakMember<HTMLSelectMenuElement> owner_select_menu_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPOVER_DATA_H_
