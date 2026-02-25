// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class GridFocusgroupStructureInfo;
class KeyboardEvent;

enum class FocusgroupDirection {
  kNone,
  kBackwardInline,
  kBackwardBlock,
  kForwardInline,
  kForwardBlock,
};

enum class FocusgroupType {
  kGrid,
  kLinear,
};

// Used to specify whether to find the first or last item in a collection.
enum class FocusgroupItemPosition { kFirst, kLast };

// Focusgroup Terminology:
//
// - Focusgroup Owner: An element with a focusgroup attribute that creates
//   an actual focusgroup (not focusgroup="none"). This element defines the
//   scope and behavior for its focusgroup items.
//
// - Focusgroup Scope: The DOM subtree under a focusgroup owner, containing
//   all potential focusgroup items. The scope ends at nested focusgroup owners
//   or opted-out subtrees (focusgroup="none").
//
// - Focusgroup Item: A focusable element within a focusgroup scope that is not
//   inside a nested focusgroup or opted-out subtree. These are the elements
//   that participate in focusgroup arrow key navigation.
//
// - Focusgroup Segment: A contiguous sequence of focusgroup items within a
//   focusgroup scope, bounded by barriers. Barriers include items in nested
//   focusgroups, opted-out subtrees, and focused native arrow key handlers.
//   Segments determine guaranteed tab stops during sequential navigation.
//
// Example:
//   <div focusgroup="toolbar">           <!-- Focusgroup Owner -->
//     <button>A</button>                 <!-- Item in segment 1 -->
//     <button>B</button>                 <!-- Item in segment 1 -->
//     <div tabindex=0 focusgroup="none"> <!-- Item in segment 1 -->
//       <button>Help</button>            <!-- Barrier -->
//     </div>
//     <button>C</button>                 <!-- Item in segment 2 -->
//     <button>D</button>                 <!-- Item in segment 2 -->
//   </div>
class CORE_EXPORT FocusgroupControllerUtils {
  STATIC_ONLY(FocusgroupControllerUtils);

 public:
  // Maps the physical arrow key from |event| to a logical focusgroup
  // direction, accounting for the writing direction (RTL, vertical writing
  // modes) of |focused_element|. The caller passes the currently focused
  // element so that arrow keys follow its local writing direction. Returns
  // kNone for non-arrow keys or when modifier keys are held.
  static FocusgroupDirection FocusgroupDirectionForEvent(
      const KeyboardEvent* event,
      const Element& focused_element);
  static bool IsDirectionBackward(FocusgroupDirection direction);
  static bool IsDirectionForward(FocusgroupDirection direction);
  static bool IsDirectionInline(FocusgroupDirection direction);
  static bool IsDirectionBlock(FocusgroupDirection direction);
  static bool IsAxisSupported(FocusgroupFlags flags,
                              FocusgroupDirection direction);
  static bool WrapsInDirection(FocusgroupFlags flags,
                               FocusgroupDirection direction);
  static Element* FindNearestFocusgroupAncestor(const Element* element,
                                                FocusgroupType type);

  // Returns the next element in the DOM tree relative to |current| in the
  // specified |direction|. If |skip_subtree| is true, the subtree of |current|
  // will be skipped. These helpers are equivalent, but use different direction
  // types for convenience.
  // This overload is is useful when the caller is managing directional
  // (arrow-key) navigation in focusgroups.
  static Element* NextElementInDirection(const Element* current,
                                         FocusgroupDirection direction,
                                         bool skip_subtree);
  // This overload is useful when the caller is managing sequential
  // (Tab/Shift+Tab) navigation in focusgroups.
  static Element* NextElementInDirection(const Element* current,
                                         mojom::blink::FocusType direction,
                                         bool skip_subtree);
  static Element* NextElement(const Element* current, bool skip_subtree);
  static Element* PreviousElement(const Element* current,
                                  bool skip_subtree = false);

  // Returns the next focusgroup item candidate within the |owner| subtree
  // relative to |current_item| in the traversal order implied by |direction|.
  // For forward directions this returns the first subsequent eligible item.
  // For backward directions this returns the last eligible item that appears
  // before |current|. Returns nullptr if none is found. Nested focusgroup
  // subtrees are skipped entirely.
  static Element* NextFocusgroupItemInDirection(const Element* owner,
                                                const Element* current_item,
                                                FocusgroupDirection direction);

  // Returns the element to focus when wrapping within |owner| in the given
  // |direction| starting from |current|. Skips nested focusgroup subtrees and
  // never returns |current| itself. Returns nullptr if no alternative item
  // exists.
  static Element* WrappedFocusgroupCandidate(const Element* owner,
                                             const Element* current,
                                             FocusgroupDirection direction);

  // Returns the first/last focusgroup item within |owner|'s scope, or nullptr
  // if no eligible item exists. |owner| must itself be a focusgroup owner.
  static Element* FocusgroupItemWithin(const Element* owner,
                                       FocusgroupItemPosition position);

  // Returns true if |element| or any of its descendants are keyboard
  // focusable. Used to determine whether an excluded subtree or nested
  // focusgroup actually creates a segment boundary (only subtrees with
  // focusable content act as barriers in the tab order).
  static bool ContainsKeyboardFocusableContent(const Element& element);

  // Returns the first/last item in the segment containing |item|, or nullptr
  // if |item| is not a focusgroup item. See class comment for segment
  // definition.
  static const Element* FocusgroupItemInSegment(
      const Element& item,
      FocusgroupItemPosition position);

  // |item| must be a focusgroup item. Returns the next item in its segment in
  // the given direction. Returns nullptr if |item| is not a focusgroup item or
  // if there is no next item in the segment in that direction.
  static const Element* NextFocusgroupItemInSegmentInDirection(
      const Element& item,
      const Element& focusgroup_owner,
      mojom::blink::FocusType direction);

  // Returns the focusgroup owner of |element| if |element| is a focusgroup
  // item, or nullptr otherwise. This combines focusgroup owner lookup with
  // validation that the element is actually a focusgroup item.
  static Element* GetFocusgroupOwnerOfItem(const Element* element);

  static bool IsFocusgroupItemWithOwner(const Element* element,
                                        const Element* focusgroup_owner);
  static bool IsGridFocusgroupItem(const Element* element);

  // Determines the guaranteed tab stop (entry element) for sequential focus
  // navigation given any focusgroup item in that segment.
  //
  // Sequential navigation (Tab/Shift+Tab) treats each focusgroup segment as
  // having one guaranteed tab stop, ensuring the segment appears once in the
  // tab order. This function determines which item in the segment should be
  // that tab stop.
  //
  // Selection priority (highest to lowest):
  // 1. Currently focused item in the segment (if any).
  // 2. Last focused item (if memory is enabled and item is in this segment).
  // 3. First item with focusgroupstart attribute.
  // 4. First item in segment.
  //
  // Note: Elements with tabindex=-1 are not focusgroup items and do not
  // participate in focusgroup navigation.
  //
  // |item|: Any focusgroup item in the segment to query.
  // |owner|: The focusgroup owner of |item| (must be valid).
  static bool IsEntryElementForFocusgroupSegment(const Element& item,
                                                 const Element& owner);
  static const Element* GetEntryElementForFocusgroupSegment(
      const Element& item,
      const Element& owner);

  // Optimized version of GetEntryElementForFocusgroupSegment that assumes
  // |first_item_in_segment| is already the first item in its segment.
  // Skips the call to FocusgroupItemInSegment to avoid redundant work.
  // |first_item_in_segment|: The first focusgroup item in the segment.
  // |owner|: The focusgroup owner of |first_item_in_segment| (must be valid).
  static const Element* GetEntryElementForFocusgroupSegmentFromFirst(
      const Element& first_item_in_segment,
      const Element& owner);

  // Returns true if the element has focusgroup="none".
  static bool HasExplicitOptOut(const Element* element);
  // Returns true if element or any ancestor (up to focusgroup root) has
  // focusgroup="none".
  static bool IsInExplicitlyOptedOutSubtree(const Element* element);

  // Returns true if |element| is itself a native arrow key handler or is
  // within a subtree rooted at one for its nearest ancestor focusgroup owner.
  // Native arrow key handlers are interactive controls whose built-in arrow
  // key behavior should take precedence over focusgroup navigation (e.g. text
  // inputs, textareas, select controls, contenteditable regions, focusable
  // scroll containers, media elements with controls, and frame elements).
  // Elements with author-defined script handlers are not considered here.
  // When this returns true, focusgroup arrow navigation should not run while
  // focus is within the handler element.
  static bool IsInArrowKeyHandler(const Element* element);

  // Returns true if |element| is in a native arrow key handler that handles
  // the specified |direction|. This allows per-axis detection, e.g., a
  // horizontal-only scroll container only handles inline (left/right)
  // navigation, not block (up/down) navigation.
  static bool IsInArrowKeyHandler(const Element& element,
                                  FocusgroupDirection direction);

  // Returns the nearest ancestor (or self) that is a native arrow key handler
  // for |element|'s nearest focusgroup owner, or nullptr if none exists.
  static const Element* GetArrowKeyHandlerRoot(const Element* element);

  // Returns true if |element| itself is an excluded subtree root:
  // 1. Has focusgroup="none" (explicit opt-out), OR
  // 2. Is the root of a focused native arrow key handler subtree.
  // These are subtrees excluded from the focusgroup scope but are not nested
  // focusgroups (which are checked separately during traversal).
  static bool IsExcludedSubtreeRoot(const Element* element);

  // Returns the nearest excluded subtree root ancestor (or self), stopping at
  // focusgroup boundaries. Returns nullptr if not in an excluded subtree.
  static const Element* FindExcludedSubtreeRoot(const Element* element);

  // Returns true if the element has the focusgroupstart attribute.
  // This boolean attribute marks an element as the preferred entry point when
  // entering a focusgroup segment via sequential focus navigation.
  static bool IsFocusgroupStart(const Element& element);

  static GridFocusgroupStructureInfo*
  CreateGridFocusgroupStructureInfoForGridRoot(const Element* root);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
