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
//   focusgroup scope, bounded by barriers. Barriers include nested focusgroup
//   owners and opted-out subtrees that contain focusable elements participating
//   in sequential focus navigation (Tab/Shift+Tab). Segments are used to
//   determine guaranteed tab stops during sequential navigation.
//
// Example:
//   <div focusgroup="toolbar">           <!-- Focusgroup Owner -->
//     <button>A</button>                 <!-- Item in segment 1 -->
//     <button>B</button>                 <!-- Item in segment 1 -->
//     <div focusgroup="none">
//       <button>Help</button>            <!-- Barrier -->
//     </div>
//     <button>C</button>                 <!-- Item in segment 2 -->
//     <button>D</button>                 <!-- Item in segment 2 -->
//   </div>
class CORE_EXPORT FocusgroupControllerUtils {
  STATIC_ONLY(FocusgroupControllerUtils);

 public:
  static FocusgroupDirection FocusgroupDirectionForEvent(
      const KeyboardEvent* event);
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
  static Element* FirstFocusgroupItemWithin(const Element* owner);
  static Element* LastFocusgroupItemWithin(const Element* owner);

  static bool DoesElementContainBarrier(const Element& element);

  // These helpers work on segments, not entire focusgroups. (see class comment
  // above for definition of segment).
  // If item is a focusgroup item, returns the first item in its segment.
  static const Element* FirstFocusgroupItemInSegment(const Element& item);
  // If item is a focusgroup item, returns the last item in its segment.
  static const Element* LastFocusgroupItemInSegment(const Element& item);

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
  // 1. Last focused item (if memory is enabled and item is in this segment).
  // 2. First item with focusgroup-entry-priority attribute.
  // 3. First item in segment.
  //
  // Note: Elements with tabindex=-1 are not focusgroup items and do not
  // participate in focusgroup navigation.
  //
  // Returns nullptr if:
  // - |item| is not a focusgroup item
  // - Another item in the segment is already focused. In this case, the
  //   segment is not eligible for a new tab stop.
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
  // Skips the call to FirstFocusgroupItemInSegment to avoid redundant work.
  // |first_item_in_segment|: The first focusgroup item in the segment.
  // |owner|: The focusgroup owner of |first_item_in_segment| (must be valid).
  static const Element* GetEntryElementForFocusgroupSegmentFromFirst(
      const Element& first_item_in_segment,
      const Element& owner);

  // Returns true if the element is opted out or within an opted-out focusgroup
  // subtree.
  static bool IsElementInOptedOutSubtree(const Element* element);
  // Returns the root of the opted-out subtree containing |element|, or nullptr
  // if |element| is not in an opted-out subtree. The root of an opted-out
  // subtree is the nearest ancestor (or self) with focusgroup="none".
  static const Element* GetOptedOutSubtreeRoot(const Element* element);

  // Returns true if the element has the focusgroup-entry-priority attribute.
  // This boolean attribute marks an element as the preferred entry point when
  // entering a focusgroup segment via sequential focus navigation.
  static bool HasFocusgroupEntryPriority(const Element& element);

  static GridFocusgroupStructureInfo*
  CreateGridFocusgroupStructureInfoForGridRoot(const Element* root);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_FOCUSGROUP_CONTROLLER_UTILS_H_
