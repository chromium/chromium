// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"

namespace blink {

using utils = FocusgroupControllerUtils;

// static
bool FocusgroupController::HandleArrowKeyboardEvent(KeyboardEvent* event,
                                                    const LocalFrame* frame) {
  DCHECK(RuntimeEnabledFeatures::FocusgroupEnabled());
  DCHECK(frame);
  FocusgroupDirection direction = utils::FocusgroupDirectionForEvent(event);
  if (direction == FocusgroupDirection::kNone)
    return false;

  if (!frame->GetDocument())
    return false;

  Element* focused = frame->GetDocument()->FocusedElement();
  if (!focused || focused != event->target()) {
    // The FocusgroupController shouldn't handle this arrow key event when the
    // focus already moved to a different element than where it came from. The
    // webpage likely had a key-handler that moved the focus.
    return false;
  }

  return Advance(focused, direction);
}

// static
bool FocusgroupController::Advance(Element* initial_element,
                                   FocusgroupDirection direction) {
  if (utils::IsDirectionForward(direction)) {
    return AdvanceForward(initial_element, direction);
  } else {
    DCHECK(utils::IsDirectionBackward(direction));
    // TODO(bebeaudr): Implement backward navigation.
  }
  return false;
}

// static
bool FocusgroupController::AdvanceForward(Element* initial_element,
                                          FocusgroupDirection direction) {
  DCHECK(initial_element);
  DCHECK(utils::IsDirectionForward(direction));

  Element* initial_focusgroup =
      utils::FindNearestFocusgroupAncestor(initial_element);
  Element* nearest_focusgroup = initial_focusgroup;
  // We only allow focusgroup navigation when we are inside of a focusgroup that
  // supports the direction.
  if (!nearest_focusgroup ||
      !utils::IsAxisSupported(nearest_focusgroup->GetFocusgroupFlags(),
                              direction)) {
    return false;
  }

  // We use the first element after the focusgroup we're in, excluding its
  // subtree, as a shortcut to determine if we exited the current focusgroup
  // without having to compute the current focusgroup ancestor on every pass.
  Element* first_element_after_focusgroup =
      utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);

  Element* current = initial_element;

  while (true) {
    // 1. Determine whether to descend in other focusgroup.
    bool skip_subtree = false;
    FocusgroupFlags current_flags = current->GetFocusgroupFlags();
    if (current_flags != FocusgroupFlags::kNone) {
      // When we're on a non-extending focusgroup, we shouldn't go into it. Same
      // for when we're at the root of an extending focusgroup that doesn't
      // support the axis of the arrow pressed.
      if (!(current_flags & FocusgroupFlags::kExtend) ||
          !utils::IsAxisSupported(current_flags, direction)) {
        skip_subtree = true;
      } else {
        nearest_focusgroup = current;
        first_element_after_focusgroup =
            utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);
      }
    }
    // 2. Move |current| to the next element.
    current = utils::NextElement(current, skip_subtree);

    // 3. When |current| is located on the next element after the focusgroup
    // we're currently in, it means that we just exited the current
    // focusgroup we were in. We need to validate that we have the right to
    // exit it, since there are a few cases that might prevent us from going
    // to the next element. See the function `CanExitFocusgroup` for more
    // details about when we shouldn't allow exiting the current focusgroup.
    //
    // When this is true, we have exited the current focusgroup we were in. If
    // we were in an extending focusgroup, we should advance to the next item in
    // the parent focusgroup if the axis is supported.
    if (current && current == first_element_after_focusgroup) {
      if (CanExitFocusgroup(nearest_focusgroup, current, initial_focusgroup,
                            direction)) {
        nearest_focusgroup = utils::FindNearestFocusgroupAncestor(current);
        first_element_after_focusgroup =
            utils::NextElement(nearest_focusgroup, /* skip_subtree */ true);
      } else {
        current = nullptr;
      }
    }

    // 4. When |current| is null, try to wrap.
    if (!current) {
      current = Wrap(nearest_focusgroup, initial_focusgroup, direction);

      if (!current) {
        // We couldn't wrap and we're out of options.
        break;
      }
    }

    // Avoid looping infinitely by breaking when the next logical element is the
    // one we started on.
    if (current == initial_element)
      break;

    // 5. |current| is finally on the next element. Focus it if it's one that
    // should be part of the focusgroup, otherwise continue the loop until it
    // finds the next item or can't find any.
    if (utils::IsFocusgroupItem(current)) {
      Focus(current, direction);
      return true;
    }
  }
  return false;
}

// static
//
// This function validates that we can exit the current focusgroup by calling
// `CanExitFocusgroupRecursive`, which validates that all ancestor focusgroups
// can be exited safely. We need to validate that the ancestor focusgroups can
// be exited only if they are exited. Here are the key scenarios where we
// prohibit a focusgroup from being exited:
// a. If we're going to an element that isn't part of a focusgroup.
// b. If we're exiting a root focusgroup (one that doesn't extend).
// c. If we're going to a focusgroup that doesn't support the direction.
// d. If we're exiting a focusgroup that should wrap.
bool FocusgroupController::CanExitFocusgroup(const Element* exiting_focusgroup,
                                             const Element* next_element,
                                             const Element* initial_focusgroup,
                                             FocusgroupDirection direction) {
  DCHECK(exiting_focusgroup);
  DCHECK(next_element);
  DCHECK(utils::NextElement(exiting_focusgroup, /*skip_subtree */ true) ==
         next_element);

  const Element* next_element_focusgroup =
      utils::FindNearestFocusgroupAncestor(next_element);
  if (!next_element_focusgroup)
    return false;

  // When we're exiting a focusgroup that can wrap, we only want to allow the
  // wrapping behavior when that focusgroup we're exiting is the same as the
  // |initial_focusgroup| or an ancestor. Otherwise, we're risking falling in an
  // infinite loop within a descendant focusgroup that wraps.
  //
  // Example:
  // <div id=root focusgroup>
  //    <span id=item1 tabindex=0></span>
  //    <div focusgroup="extend wrap">
  //      <span id=item2></span> <!--NOT FOCUSABLE-->
  //      <span id=item3></span> <!--NOT FOCUSABLE-->
  //    </div>
  //    <span id=item4 tabindex=-1></span>
  // </div>
  //
  // Without this condition, we would treat the wrapping behavior on the inner
  // focusgroup as legit and iterate for ever between the two non-focusable
  // items ever though the focus never actually landed in that descendant
  // focusgroup.
  bool wraps =
      utils::WrapsInDirection(exiting_focusgroup->GetFocusgroupFlags(),
                              direction) &&
      FlatTreeTraversal::CommonAncestor(
          *exiting_focusgroup, *initial_focusgroup) == exiting_focusgroup;
  return CanExitFocusgroupRecursive(exiting_focusgroup, next_element, direction,
                                    wraps);
}

// static
bool FocusgroupController::CanExitFocusgroupRecursive(
    const Element* exiting_focusgroup,
    const Element* next_element,
    FocusgroupDirection direction,
    bool check_wrap) {
  DCHECK(exiting_focusgroup);
  DCHECK(next_element);

  // When this is true, we are not exiting |exiting_focusgroup| and thus won't
  // be exiting any ancestor focusgroup.
  if (utils::NextElement(exiting_focusgroup, /* skip_subtree */ true) !=
      next_element) {
    return true;
  }

  FocusgroupFlags exiting_focusgroup_flags =
      exiting_focusgroup->GetFocusgroupFlags();
  DCHECK(exiting_focusgroup_flags != FocusgroupFlags::kNone);

  if (!(exiting_focusgroup_flags & FocusgroupFlags::kExtend))
    return false;

  const Element* parent_focusgroup =
      utils::FindNearestFocusgroupAncestor(exiting_focusgroup);
  FocusgroupFlags parent_focusgroup_flags =
      parent_focusgroup ? parent_focusgroup->GetFocusgroupFlags()
                        : FocusgroupFlags::kNone;

  DCHECK(utils::IsAxisSupported(exiting_focusgroup_flags, direction));
  if (!utils::IsAxisSupported(parent_focusgroup_flags, direction))
    return false;

  if (check_wrap) {
    DCHECK(utils::WrapsInDirection(exiting_focusgroup_flags, direction));
    if (!utils::WrapsInDirection(parent_focusgroup_flags, direction))
      return false;
  }

  return CanExitFocusgroupRecursive(parent_focusgroup, next_element, direction,
                                    check_wrap);
}

// static
Element* FocusgroupController::Wrap(Element* nearest_focusgroup,
                                    const Element* initial_focusgroup,
                                    FocusgroupDirection direction) {
  // 1. Get the focusgroup that initiates the wrapping scope in this axis. We
  // need to go up to the root-most focusgroup in order to be able to get the
  // "next" element, ie. the first item of this focusgroup. Stopping at the
  // first focusgroup that supports wrapping in that axis would break the
  // extend behavior and return the wrong element.
  Element* fg_wrap_root = nullptr;
  for (Element* fg = nearest_focusgroup; fg;
       fg = utils::FindNearestFocusgroupAncestor(fg)) {
    FocusgroupFlags flags = fg->GetFocusgroupFlags();
    if (!utils::WrapsInDirection(flags, direction))
      break;

    fg_wrap_root = fg;

    if (!(flags & FocusgroupFlags::kExtend))
      break;
  }

  // 2. There are no next valid element and we can't wrap - `AdvanceForward`
  // should fail.
  if (!fg_wrap_root)
    return nullptr;

  // 3. Only allow wrapping when the |fg_wrap_root| is the initial focusgroup or
  // an ancestor of it - prevent wrapping in a descendant focusgroup, as this
  // could lead in an infinite loop if that descendant focusgroup doesn't have
  // an focusgroup item.
  if (FlatTreeTraversal::CommonAncestor(*fg_wrap_root, *initial_focusgroup) !=
      fg_wrap_root) {
    return nullptr;
  }

  // 4. Set the focus on the first element within the subtree of the
  // current focusgroup.
  return utils::NextElement(fg_wrap_root, /* skip_subtree */ false);
}

// static
void FocusgroupController::Focus(Element* element,
                                 FocusgroupDirection direction) {
  DCHECK(element);
  element->focus(FocusParams(SelectionBehaviorOnFocus::kReset,
                             utils::IsDirectionForward(direction)
                                 ? mojom::blink::FocusType::kForward
                                 : mojom::blink::FocusType::kBackward,
                             nullptr));
}

}  // namespace blink