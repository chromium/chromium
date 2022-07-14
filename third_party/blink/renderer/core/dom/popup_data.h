// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/popup_animation_finished_event_listener.h"
#include "third_party/blink/renderer/core/html/forms/html_select_menu_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

enum class PopupVisibilityState {
  kHidden,
  kTransitioning,
  kShowing,
};

class PopupData final : public GarbageCollected<PopupData> {
 public:
  PopupData() = default;
  PopupData(const PopupData&) = delete;
  PopupData& operator=(const PopupData&) = delete;

  bool hadDefaultOpenWhenParsed() const { return had_defaultopen_when_parsed_; }
  void setHadDefaultOpenWhenParsed(bool value) {
    had_defaultopen_when_parsed_ = value;
  }

  PopupVisibilityState visibilityState() const { return visibility_state_; }
  void setVisibilityState(PopupVisibilityState visibility_state) {
    visibility_state_ = visibility_state;
  }

  PopupValueType type() const { return type_; }
  void setType(PopupValueType type) {
    type_ = type;
    DCHECK_NE(type, PopupValueType::kNone)
        << "Remove PopupData rather than setting kNone type";
  }

  HidePopupFocusBehavior focusBehavior() const { return focus_behavior_; }
  void setFocusBehavior(HidePopupFocusBehavior focus_behavior) {
    focus_behavior_ = focus_behavior;
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

  PopupAnimationFinishedEventListener* animationFinishedListener() const {
    return animation_finished_listener_;
  }
  void setAnimationFinishedListener(
      PopupAnimationFinishedEventListener* listener) {
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

  HeapHashMap<WeakMember<Element>, TaskHandle>& hoverPopupTasks() {
    return hover_popup_tasks_;
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
    visitor->Trace(hover_popup_tasks_);
    visitor->Trace(owner_select_menu_element_);
  }

 private:
  bool had_defaultopen_when_parsed_ = false;
  PopupVisibilityState visibility_state_ = PopupVisibilityState::kHidden;
  PopupValueType type_ = PopupValueType::kNone;
  HidePopupFocusBehavior focus_behavior_ = HidePopupFocusBehavior::kNone;
  WeakMember<Element> invoker_;
  WeakMember<Element> previously_focused_element_;
  // We hold a strong reference to the animation finished listener, so that we
  // can confirm that the listeners get removed before cleanup.
  Member<PopupAnimationFinishedEventListener> animation_finished_listener_;
  // Map from triggering elements to a TaskHandle for the task that will invoke
  // the pop-up.
  HeapHashMap<WeakMember<Element>, TaskHandle> hover_popup_tasks_;

  // TODO(crbug.com/1197720): The popup position should be provided by the new
  // anchored positioning scheme.
  bool needs_repositioning_for_select_menu_ = false;
  WeakMember<HTMLSelectMenuElement> owner_select_menu_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_POPUP_DATA_H_
