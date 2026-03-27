// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_SAFE_TRIANGLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_SAFE_TRIANGLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

class HTMLMenuItemElement;
class HTMLMenuListElement;

// MenuSafeTriangle manages the things that happen when a menulist is open,
// the mouse pointer is outside of the menulist, and the user wants to move
// the mouse to the menulist, diagonally across other menuitems that would
// normally cause the menulist to close.  In these cases we create a "safe
// triangle" area where, for a short period of time, we suppress (but buffer,
// if we need to run them later) the closing of the destination menulist and
// the opening of its siblings.
//
// The class is responsible for:
// * deciding when we need to create a safe triangle and what area it covers,
// * deciding which interest-gained and interest-lost should be suppressed
//   (and buffered in case they need to happen later), and
// * deciding when we no longer need the safe triangle.
//
// As discussed in https://github.com/openui/open-ui/issues/1190 the details
// of this behavior are implementation-defined.
class CORE_EXPORT MenuSafeTriangle final
    : public GarbageCollected<MenuSafeTriangle> {
 public:
  MenuSafeTriangle(HTMLMenuItemElement* invoker_menu_item,
                   HTMLMenuListElement* invoked_submenu,
                   const gfx::QuadF& triangle,
                   const gfx::QuadF& submenu_quad);

  static void MaybeCreate(HTMLMenuItemElement* invoker_menu_item,
                          HTMLMenuListElement* invoked_submenu);

  void Trace(Visitor* visitor) const;

  void ExpireTimerFired(TimerBase* timer);

  // Recheck the conditions (mouse position, popover-open state) for whether
  // this safe triangle should continue to exist (and continue deferring
  // interest gains and losses).
  void Recheck();

  bool ShouldDeferInterestGained(Element* invoker,
                                 Element* target,
                                 Element::InterestState);
  bool ShouldDeferInterestLost(Element* invoker,
                               Element* target,
                               Element::InterestLostCancelable,
                               Element::InterestLostPopoverBehavior);

 private:
  void Finish(bool from_timer = false);

  struct InterestGainedData : public GarbageCollected<InterestGainedData> {
    Member<Element> invoker;
    Member<Element> target;
    // TODO(https://crbug.com/406566432): We currently preserve state as is
    // (from the most recent interest gain) when we defer a call, but it's
    // possible we want some other coalescing behavior or change when we're
    // deferring.
    Element::InterestState state;

    InterestGainedData(Element* invoker_arg,
                       Element* target_arg,
                       Element::InterestState state_arg)
        : invoker(invoker_arg), target(target_arg), state(state_arg) {}
    void Trace(Visitor* visitor) const;
  };
  struct InterestLostData : public GarbageCollected<InterestLostData> {
    Member<Element> invoker;
    Member<Element> target;
    // TODO(https://crbug.com/406566432): We currently preserve cancelable and
    // behavior as is (from the most recent interest gain) when we defer a
    // call, but it's possible we want some other coalescing behavior or
    // change when we're deferring.
    Element::InterestLostCancelable cancelable;
    Element::InterestLostPopoverBehavior behavior;

    InterestLostData(Element* invoker_arg,
                     Element* target_arg,
                     Element::InterestLostCancelable cancelable_arg,
                     Element::InterestLostPopoverBehavior behavior_arg)
        : invoker(invoker_arg),
          target(target_arg),
          cancelable(cancelable_arg),
          behavior(behavior_arg) {}
    void Trace(Visitor* visitor) const;
  };

  Member<HTMLMenuItemElement> invoker_menu_item_;
  Member<HTMLMenuListElement> invoked_submenu_;
  gfx::QuadF triangle_;
  gfx::QuadF submenu_quad_;

  // The interest-gained and interest-lost actions that we suppressed while
  // the safe triangle was active, in case we need to fire them when it goes
  // away, in the order they originally occurred.  We do not store both a gain
  // and a loss for the same element; we remove the opposite rather than
  // storing both.
  VectorOf<InterestGainedData> deferred_interest_gained_;
  VectorOf<InterestLostData> deferred_interest_lost_;

  HeapTaskRunnerTimer<MenuSafeTriangle> expire_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MENU_SAFE_TRIANGLE_H_
